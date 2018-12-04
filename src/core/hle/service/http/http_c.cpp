// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <LUrlParser.h>
#include <cryptopp/aes.h>
#include <cryptopp/base64.h>
#include <cryptopp/modes.h>
#include <fmt/format.h>
#include <httplib.h>
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/romfs.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/http/http_c.h"
#include "core/hw/aes/key.h"
#include "core/settings.h"

namespace Service::HTTP {

namespace ErrCodes {

enum {
    InvalidRequestState = 22,
    TooManyContexts = 26,
    InvalidRequestMethod = 32,
    HeaderNotFound = 40,
    NoNetworkConnection = 70,
    ContextNotFound = 100,

    /// This error is returned in multiple situations: when trying to initialize an
    /// already-initialized session, or when using the wrong context handle in a context-bound
    /// session
    SessionStateError = 102,
    TooManyClientCerts = 203,
    NotImplemented = 1012,
};

} // namespace ErrCodes

const ResultCode ERROR_STATE_ERROR{// 0xD8A0A066
                                   ResultCode(ErrCodes::SessionStateError, ErrorModule::HTTP,
                                              ErrorSummary::InvalidState, ErrorLevel::Permanent)};
const ResultCode ERROR_NOT_IMPLEMENTED{// 0xD960A3F4
                                       ResultCode(ErrCodes::NotImplemented, ErrorModule::HTTP,
                                                  ErrorSummary::Internal, ErrorLevel::Permanent)};
const ResultCode ERROR_TOO_MANY_CLIENT_CERTS{
    // 0xD8A0A0CB
    ResultCode(ErrCodes::TooManyClientCerts, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent)};
const ResultCode ERROR_WRONG_CERT_ID{
    // 0xD8E0B839
    ResultCode(57, ErrorModule::SSL, ErrorSummary::InvalidArgument, ErrorLevel::Permanent)};
const ResultCode ERROR_HEADER_NOT_FOUND{
    // 0xD8A0A028
    ResultCode(ErrCodes::HeaderNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent)};
const ResultCode ERROR_NO_NETWORK_CONNECTION{
    // 0xD8A0A046
    ResultCode(ErrCodes::NoNetworkConnection, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent)};
const ResultCode RESULT_DOWNLOADPENDING{
    // 0xD840A02B
    ResultCode(43, ErrorModule::HTTP, ErrorSummary::WouldBlock, ErrorLevel::Permanent)};

u32 Context::GetResponseContentLength() const {
    if (response.HasHeader("Content-Length"))
        return std::stoi(response.GetHeader("Content-Length"));
    return 0;
}

void Context::Send() {
    client->set_verify(!((ssl_config.options & 0x200) == 0x200));
    httplib::Request req;
    static const std::unordered_map<RequestMethod, std::string> method_string_map{{
        {RequestMethod::Get, "GET"},
        {RequestMethod::Post, "POST"},
        {RequestMethod::Head, "HEAD"},
        {RequestMethod::Put, "PUT"},
        {RequestMethod::Delete, "DELETE"},
        {RequestMethod::PostEmpty, "POST"},
        {RequestMethod::PutEmpty, "PUT"},
    }};
    static const std::unordered_map<RequestMethod, bool> method_body_map{{
        {RequestMethod::Get, false},
        {RequestMethod::Post, true},
        {RequestMethod::Head, false},
        {RequestMethod::Put, true},
        {RequestMethod::Delete, true},
        {RequestMethod::PostEmpty, false},
        {RequestMethod::PutEmpty, false},
    }};
    req.method = method_string_map.find(method)->second;
    req.path = '/' + path;
    for (const auto& header : headers)
        req.headers.emplace(header.first, header.second);
    if (method_body_map.find(method)->second) {
        bool first{true};
        for (const auto& item : post_data) {
            switch (item.type) {
            case PostData::Type::Ascii:
                if (!first)
                    req.body += '&';
                req.body += fmt::format("{}={}", httplib::detail::encode_url(item.ascii.name),
                                        httplib::detail::encode_url(item.ascii.value));
                break;
            case PostData::Type::Binary:
                if (!first)
                    req.body += '&';
                req.body +=
                    fmt::format("{}={}", httplib::detail::encode_url(item.binary.name),
                                httplib::detail::encode_url(std::string(
                                    reinterpret_cast<const char*>(item.binary.data.data()))));
                break;
            case PostData::Type::Raw:
                req.body += item.raw.data;
                break;
            }
            first = false;
        }
    }
    httplib::Response res;
    if (client->send(req, res)) {
        response.status_code = static_cast<u32>(res.status);
        response.body = res.body;
        response.raw = GetRawResponseWithoutBody();
        response.headers.clear();
        for (const auto& header : res.headers)
            response.headers[header.first] = header.second;
        LOG_DEBUG(Service_HTTP, "Raw response: {}", response.raw.append(1, '\n').append(res.body));
    } else {
        response.status_code = 500;
        response.body.clear();
        response.raw.clear();
        response.headers.clear();
    }
}

void Context::SetKeepAlive(bool enable) {
    headers["Connection"] = enable ? "keep-alive" : "close";
}

std::string Context::GetRawResponseWithoutBody() const {
    auto str{fmt::format("HTTP/1.1 {} ", response.status_code)};
    switch (response.status_code) {
    case 100:
        str += "Continue";
        break;
    case 101:
        str += "Switching Protocols";
        break;
    case 200:
        str += "OK";
        break;
    case 201:
        str += "Created";
        break;
    case 202:
        str += "Accepted";
        break;
    case 203:
        str += "Non-Authoritative Information";
        break;
    case 204:
        str += "No Content";
        break;
    case 205:
        str += "Reset Content";
        break;
    case 206:
        str += "Partial Content";
        break;
    case 300:
        str += "Multiple Choices";
        break;
    case 301:
        str += "Moved Permanently";
        break;
    case 302:
        str += "Found";
        break;
    case 303:
        str += "See Other";
        break;
    case 304:
        str += "Not Modified";
        break;
    case 305:
        str += "Use Proxy";
        break;
    case 307:
        str += "Temporary Redirect";
        break;
    case 400:
        str += "Bad Request";
        break;
    case 401:
        str += "Unauthorized";
        break;
    case 402:
        str += "Payment Required";
        break;
    case 403:
        str += "Forbidden";
        break;
    case 404:
        str += "Not Found";
        break;
    case 405:
        str += "Method Not Allowed";
        break;
    case 406:
        str += "Not Acceptable";
        break;
    case 407:
        str += "Proxy Authentication Required";
        break;
    case 408:
        str += "Request Timeout";
        break;
    case 409:
        str += "Conflict";
        break;
    case 410:
        str += "Gone";
        break;
    case 411:
        str += "Length Required";
        break;
    case 412:
        str += "Precondition Failed";
        break;
    case 413:
        str += "Payload Too Large";
        break;
    case 414:
        str += "URI Too Long";
        break;
    case 415:
        str += "Unsupported Media Type";
        break;
    case 416:
        str += "Requested Range Not Satisfiable";
        break;
    case 417:
        str += "Expectation Failed";
        break;
    case 426:
        str += "Upgrade Required";
        break;
    case 500:
        str += "Internal Server Error";
        break;
    case 501:
        str += "Not Implemented";
        break;
    case 502:
        str += "Bad Gateway";
        break;
    case 503:
        str += "Service Unavailable";
        break;
    case 504:
        str += "Gateway Timeout";
        break;
    case 505:
        str += "HTTP Version Not Supported";
        break;
    }
    str += "\r\n";
    for (const auto& header : response.headers) {
        str += header.first;
        str += ": ";
        str += header.second;
        str += "\r\n";
    }
    return str;
}

void HTTP_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1, 1, 4};
    const u32 shmem_size{rp.Pop<u32>()};
    rp.PopPID();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();
    if (shared_memory)
        shared_memory->SetName("http:C Shared Memory");
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_STATE_ERROR);
        return;
    }
    session_data->initialized = true;
    session_data->session_id = ++session_counter;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(Settings::values.n_wifi_status == 0 ? ERROR_NO_NETWORK_CONNECTION : RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "shmem_size={}", shmem_size);
}

void HTTP_C::InitializeConnectionSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x8, 1, 2};
    const Context::Handle context_handle{rp.Pop<u32>()};
    rp.PopPID();
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_STATE_ERROR);
        return;
    }
    // TODO: Check that the input PID matches the PID that created the context.
    auto itr{contexts.find(context_handle)};
    if (itr == contexts.end()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::ContextNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        return;
    }
    session_data->initialized = true;
    session_data->session_id = ++session_counter;
    // Bind the context to the current session.
    session_data->current_http_context = context_handle;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_handle={}", context_handle);
}

void HTTP_C::CreateContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x2, 2, 2};
    const u32 url_size{rp.Pop<u32>()};
    const auto method{rp.PopEnum<RequestMethod>()};
    auto& buffer{rp.PopMappedBuffer()};
    // Copy the buffer into a string without the \0 at the end of the buffer
    std::string url(url_size, '\0');
    buffer.Read(&url[0], 0, url_size - 1);
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to create a context on an uninitialized session");
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ERROR_STATE_ERROR);
        rb.PushMappedBuffer(buffer);
        return;
    }
    // This command can only be called without a bound session.
    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrorDescription::NotImplemented, ErrorModule::HTTP,
                           ErrorSummary::Internal, ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }
    constexpr std::size_t MaxConcurrentHTTPContexts{8};
    if (session_data->num_http_contexts >= MaxConcurrentHTTPContexts) {
        // There can only be 8 HTTP contexts open at the same time for any particular session.
        LOG_ERROR(Service_HTTP, "Tried to open too many HTTP contexts");
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrCodes::TooManyContexts, ErrorModule::HTTP, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }
    if (method == RequestMethod::None || static_cast<u32>(method) >= TotalRequestMethods) {
        LOG_ERROR(Service_HTTP, "invalid request method {}", static_cast<u32>(method));
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidRequestMethod, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }
    auto& context{contexts.emplace(++context_counter, Context{}).first->second};
    auto parsed_url{LUrlParser::clParseURL::ParseURL(url)};
    int port;
    if (parsed_url.m_Scheme == "http") {
        if (!parsed_url.GetPort(&port))
            port = 80;
        context.client = std::make_unique<httplib::Client>(parsed_url.m_Host.c_str(), port);
    } else if (parsed_url.m_Scheme == "https") {
        if (!parsed_url.GetPort(&port))
            port = 443;
        context.client = std::make_unique<httplib::SSLClient>(parsed_url.m_Host.c_str(), port);
    } else
        UNREACHABLE_MSG("Invalid scheme!");
    context.path = parsed_url.m_Path;
    context.method = method;
    context.state = RequestState::NotStarted;
    // TODO: Find a correct default value for this field.
    context.socket_buffer_size = 0;
    context.handle = context_counter;
    context.session_id = session_data->session_id;
    ++session_data->num_http_contexts;
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(context_counter);
    rb.PushMappedBuffer(buffer);
    LOG_DEBUG(Service_HTTP, "url_size={}, url={}, method={}", url_size, std::move(url),
              static_cast<u32>(method));
}

void HTTP_C::CloseContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x3, 1, 0};
    u32 context_handle{rp.Pop<u32>()};
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to close a context on an uninitialized session");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_STATE_ERROR);
        return;
    }
    if (session_data->current_http_context)
        session_data->current_http_context.reset();
    auto itr{contexts.find(context_handle)};
    if (itr == contexts.end()) {
        // The real HTTP module just silently fails in this case.
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_ERROR(Service_HTTP, "context {} not found", context_handle);
        return;
    }
    // TODO: Make sure that only the session that created the context can close it.
    contexts.erase(itr);
    --context_counter;
    --session_data->num_http_contexts;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_handle={}", context_handle);
}

void HTTP_C::AddRequestHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x11, 3, 4};
    const u32 context_handle{rp.Pop<u32>()};
    const u32 name_size{rp.Pop<u32>()};
    const u32 value_size{rp.Pop<u32>()};
    const auto name_buffer{rp.PopStaticBuffer()};
    auto& value_buffer{rp.PopMappedBuffer()};
    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);
    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(&value[0], 0, value_size - 1);
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to add a request header on an uninitialized session");
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ERROR_STATE_ERROR);
        rb.PushMappedBuffer(value_buffer);
        return;
    }
    // This command can only be called with a bound context
    if (!session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called without a bound context");
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrorDescription::NotImplemented, ErrorModule::HTTP,
                           ErrorSummary::Internal, ErrorLevel::Permanent));
        rb.PushMappedBuffer(value_buffer);
        return;
    }
    if (session_data->current_http_context != context_handle) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add a request header on a mismatched session input context={}, session "
                  "context={}",
                  context_handle, *session_data->current_http_context);
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ERROR_STATE_ERROR);
        rb.PushMappedBuffer(value_buffer);
        return;
    }
    auto itr{contexts.find(context_handle)};
    ASSERT(itr != contexts.end());
    if (itr->second.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add a request header on a context that has already been started.");
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidRequestState, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        rb.PushMappedBuffer(value_buffer);
        return;
    }
    itr->second.headers[name] = value;
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(value_buffer);
    LOG_DEBUG(Service_HTTP, "name={}, value={}, context_handle={}", name, value, context_handle);
}

void HTTP_C::OpenClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x32, 2, 4};
    u32 cert_size{rp.Pop<u32>()};
    u32 key_size{rp.Pop<u32>()};
    auto& cert_buffer{rp.PopMappedBuffer()};
    auto& key_buffer{rp.PopMappedBuffer()};
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    auto result{RESULT_SUCCESS};
    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Command called with uninitialized session");
        result = ERROR_STATE_ERROR;
    } else if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        result = ERROR_NOT_IMPLEMENTED;
    } else if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        result = ERROR_TOO_MANY_CLIENT_CERTS;
    } else {
        auto& cert{client_certs.emplace(++client_certs_counter, ClientCertContext{}).first->second};
        cert.handle = client_certs_counter;
        cert.certificate.resize(cert_size);
        cert_buffer.Read(&cert.certificate[0], 0, cert_size);
        cert.private_key.resize(key_size);
        cert_buffer.Read(&cert.private_key[0], 0, key_size);
        cert.session_id = session_data->session_id;
        ++session_data->num_client_certs;
    }
    LOG_DEBUG(Service_HTTP, "cert_size={}, key_size={}", cert_size, key_size);
    auto rb{rp.MakeBuilder(2, 4)};
    rb.Push(result);
    rb.Push<u32>(client_certs_counter);
    rb.PushMappedBuffer(cert_buffer);
    rb.PushMappedBuffer(key_buffer);
}

void HTTP_C::OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x33, 1, 0};
    u8 cert_id{rp.Pop<u8>()};
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Command called with uninitialized session");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_STATE_ERROR);
        return;
    }
    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_NOT_IMPLEMENTED);
        return;
    }
    if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_TOO_MANY_CLIENT_CERTS);
        return;
    }
    constexpr u8 default_cert_id{0x40};
    if (cert_id != default_cert_id) {
        LOG_ERROR(Service_HTTP, "called with invalid cert_id {}", cert_id);
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_WRONG_CERT_ID);
        return;
    }
    if (!ClCertA.init) {
        LOG_ERROR(Service_HTTP, "called but ClCertA is missing");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(static_cast<ResultCode>(-1));
        return;
    }
    const auto& it{std::find_if(client_certs.begin(), client_certs.end(),
                                [default_cert_id, &session_data](const auto& i) {
                                    return default_cert_id == i.second.cert_id &&
                                           session_data->session_id == i.second.session_id;
                                })};
    if (it != client_certs.end()) {
        auto rb{rp.MakeBuilder(2, 0)};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(it->first);
        LOG_DEBUG(Service_HTTP, "called with an already loaded cert_id={}", cert_id);
        return;
    }
    auto& cert{client_certs.emplace(++client_certs_counter, ClientCertContext{}).first->second};
    cert.handle = client_certs_counter;
    cert.certificate = ClCertA.certificate;
    cert.private_key = ClCertA.private_key;
    cert.session_id = session_data->session_id;
    ++session_data->num_client_certs;
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(client_certs_counter);
    LOG_DEBUG(Service_HTTP, "cert_id={}", cert_id);
}

void HTTP_C::CloseClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x34, 1, 0};
    ClientCertContext::Handle cert_handle{rp.Pop<u32>()};
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    if (client_certs.find(cert_handle) == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "Command called with a unknown client cert handle {}", cert_handle);
        auto rb{rp.MakeBuilder(1, 0)};
        // This just return success without doing anything
        rb.Push(RESULT_SUCCESS);
        return;
    }
    if (client_certs[cert_handle].session_id != session_data->session_id) {
        LOG_ERROR(Service_HTTP, "called from another main session");
        auto rb{rp.MakeBuilder(1, 0)};
        // This just return success without doing anything
        rb.Push(RESULT_SUCCESS);
        return;
    }
    client_certs.erase(cert_handle);
    --client_certs_counter;
    --session_data->num_client_certs;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "cert_handle={}", cert_handle);
}

void HTTP_C::GetRequestState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x5, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum<RequestState>(itr->second.state);
    LOG_DEBUG(Service_HTTP, "state={}", static_cast<u32>(itr->second.state));
}

void HTTP_C::GetDownloadSizeState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x6, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    u32 content_length{itr->second.GetResponseContentLength()};
    auto rb{rp.MakeBuilder(3, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(itr->second.current_offset);
    rb.Push<u32>(content_length);
    LOG_TRACE(Service_HTTP, "current_offset={}, content_length={}", itr->second.current_offset,
              content_length);
}

void HTTP_C::AddPostDataAscii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x12, 3, 4};
    const u32 context_id{rp.Pop<u32>()};
    const u32 name_length{rp.Pop<u32>()};
    const u32 value_length{rp.Pop<u32>()};
    const auto name_buffer{rp.PopStaticBuffer()};
    std::string name(value_length, '\0');
    std::memcpy(&name[0], name_buffer.data(), name_length);
    auto& value_buffer{rp.PopMappedBuffer()};
    std::string value(value_length, '\0');
    value_buffer.Read(&value[0], 0, value_length);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    using PostData = Context::PostData;
    PostData post_data;
    post_data.type = PostData::Type::Ascii;
    post_data.ascii.name = name;
    post_data.ascii.value = value;
    itr->second.post_data.push_back(post_data);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, name={}, value={}", context_id, name, value);
}

void HTTP_C::AddPostDataBinary(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x13, 3, 4};
    const u32 context_id{rp.Pop<u32>()};
    const u32 name_length{rp.Pop<u32>()};
    const u32 data_size{rp.Pop<u32>()};
    const auto name_buffer{rp.PopStaticBuffer()};
    std::string name(name_length, '\0');
    std::memcpy(&name[0], name_buffer.data(), name_length);
    auto& data_buffer{rp.PopMappedBuffer()};
    std::vector<u8> data(data_size);
    data_buffer.Read(data.data(), 0, data_size);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    using PostData = Context::PostData;
    PostData post_data;
    post_data.type = PostData::Type::Binary;
    post_data.binary.name = name;
    post_data.binary.data = data;
    itr->second.post_data.push_back(post_data);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, name={}, data={}", context_id, name,
              std::string(reinterpret_cast<const char*>(data.data())));
}

void HTTP_C::AddPostDataRaw(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x14, 2, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 length{rp.Pop<u32>()};
    auto& buffer{rp.PopMappedBuffer()};
    std::string data(length, '\0');
    buffer.Read(&data[0], 0, length);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    using PostData = Context::PostData;
    PostData post_data;
    post_data.type = PostData::Type::Raw;
    post_data.raw.data = data;
    itr->second.post_data.push_back(post_data);
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
    LOG_DEBUG(Service_HTTP, "context_id={}, data={}", context_id, data);
}

void HTTP_C::SetPostDataType(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x15, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u8 type{rp.Pop<u8>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_HTTP, "(stubbed) context_id={}, type={}", context_id,
                static_cast<u32>(type));
}

void HTTP_C::SendPOSTDataRawTimeout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1B, 4, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 buffer_size{rp.Pop<u32>()};
    const u64 timeout{rp.Pop<u64>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    std::string data(buffer_size, '\0');
    buffer.Read(&data[0], 0, buffer_size);
    using PostData = Context::PostData;
    PostData post_data;
    post_data.type = PostData::Type::Raw;
    post_data.raw.data = data;
    itr->second.post_data.push_back(post_data);
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
    LOG_DEBUG(Service_HTTP, "context_id={}, buffer_size={}, timeout={}, data={}", context_id,
              buffer_size, timeout, data);
}

void HTTP_C::NotifyFinishSendPostData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1D, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_HTTP, "(stubbed) context_id={}", context_id);
}

void HTTP_C::GetResponseHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1E, 3, 4};
    const u32 context_id{rp.Pop<u32>()};
    const u32 name_size{rp.Pop<u32>()};
    const u32 value_max_size{rp.Pop<u32>()};
    const auto name_buffer{rp.PopStaticBuffer()};
    auto& value_buffer{rp.PopMappedBuffer()};
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    if (itr->second.response.HasHeader(name)) {
        auto value{itr->second.response.GetHeader(name)};
        if (value.length() > value_max_size)
            value.resize(value_max_size);
        value_buffer.Write(value.c_str(), 0, value.length());
        auto rb{rp.MakeBuilder(2, 2)};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(value.length());
        rb.PushMappedBuffer(value_buffer);
        LOG_DEBUG(Service_HTTP, "name={}, name_size={}, value={}, value_max_size={}, context_id={}",
                  name, name_size, value, value_max_size, context_id);
    } else {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_HEADER_NOT_FOUND);
        LOG_ERROR(Service_HTTP,
                  "Header not found (name={}, name_size={}, value_max_size={}, context_id={})",
                  name, name_size, value_max_size, context_id);
    }
}

void HTTP_C::GetResponseHeaderTimeout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1F, 5, 4};
    const u32 context_id{rp.Pop<u32>()};
    const u32 name_size{rp.Pop<u32>()};
    const u32 value_max_size{rp.Pop<u32>()};
    const u64 timeout{rp.Pop<u64>()};
    const auto name_buffer{rp.PopStaticBuffer()};
    auto& value_buffer{rp.PopMappedBuffer()};
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    if (itr->second.response.HasHeader(name)) {
        auto value{itr->second.response.GetHeader(name)};
        if (value.length() > value_max_size)
            value.resize(value_max_size);
        value_buffer.Write(value.c_str(), 0, value.length());
        auto rb{rp.MakeBuilder(2, 2)};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(value.length());
        rb.PushMappedBuffer(value_buffer);
        LOG_DEBUG(Service_HTTP,
                  "name={}, name_size={}, value={}, value_max_size={}, timeout={}, context_id={}",
                  name, name_size, value, value_max_size, timeout, context_id);
    } else {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_HEADER_NOT_FOUND);
        LOG_ERROR(Service_HTTP,
                  "Header not found (name={}, name_size={}, value_max_size={}, timeout={}, "
                  "context_id={})",
                  name, name_size, value_max_size, timeout, context_id);
    }
}

void HTTP_C::GetResponseData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x20, 2, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 max_buffer_size{rp.Pop<u32>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto raw{itr->second.GetRawResponseWithoutBody()};
    if (raw.length() > max_buffer_size)
        raw.resize(max_buffer_size);
    buffer.Write(raw.c_str(), 0, raw.length());
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(raw.length());
    rb.PushMappedBuffer(buffer);
    LOG_DEBUG(Service_HTTP, "context_id={}, max_buffer_size={}", context_id, max_buffer_size);
}

void HTTP_C::GetResponseDataTimeout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x21, 4, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 max_buffer_size{rp.Pop<u32>()};
    const u64 timeout{rp.Pop<u64>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto raw{itr->second.GetRawResponseWithoutBody()};
    if (raw.length() > max_buffer_size)
        raw.resize(max_buffer_size);
    buffer.Write(raw.c_str(), 0, raw.length());
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(raw.length());
    rb.PushMappedBuffer(buffer);
    LOG_DEBUG(Service_HTTP, "context_id={}, max_buffer_size={}, timeout={}", context_id,
              max_buffer_size, timeout);
}

void HTTP_C::GetResponseStatusCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x22, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(itr->second.response.status_code);
    LOG_DEBUG(Service_HTTP, "context_id={}, status_code={}", context_id,
              itr->second.response.status_code);
}

void HTTP_C::GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x23, 3, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u64 timeout{rp.Pop<u64>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(itr->second.response.status_code);
    LOG_DEBUG(Service_HTTP, "context_id={}, timeout={}, status_code={}", context_id, timeout,
              itr->second.response.status_code);
}

void HTTP_C::AddTrustedRootCA(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x24, 2, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 buffer_size{rp.Pop<u32>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.ssl_config.enable_root_cert_chain = true;
    RootCertChain::RootCACert cert;
    cert.session_id = session_data->session_id;
    cert.handle = ++itr->second.ssl_config.root_ca_chain.certs_counter;
    cert.certificate.resize(buffer_size);
    buffer.Read(cert.certificate.data(), 0, buffer_size);
    itr->second.ssl_config.root_ca_chain.certificates.push_back(cert);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, buffer_size={}", context_id, buffer_size);
}

void HTTP_C::AddDefaultCert(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x25, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 cert_id{rp.Pop<u32>()};
    ASSERT(cert_id <= 0xB);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.ssl_config.enable_root_cert_chain = true;
    RootCertChain::RootCACert cert;
    cert.session_id = session_data->session_id;
    cert.handle = ++itr->second.ssl_config.root_ca_chain.certs_counter;
    cert.certificate = default_root_certs[cert_id].certificate;
    itr->second.ssl_config.root_ca_chain.certificates.push_back(cert);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, cert_id={}", context_id, cert_id);
}

void HTTP_C::RootCertChainAddCert(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x2F, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 buffer_size{rp.Pop<u32>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{root_cert_chains.find(context_id)};
    ASSERT(itr != root_cert_chains.end());
    RootCertChain::RootCACert cert;
    cert.session_id = session_data->session_id;
    cert.handle = ++itr->second.certs_counter;
    buffer.Read(cert.certificate.data(), 0, buffer_size);
    itr->second.certificates.push_back(cert);
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(itr->second.certs_counter);
    LOG_DEBUG(Service_HTTP, "context_id={}, buffer_size={}, cert_id={}", context_id, buffer_size,
              itr->second.certs_counter);
}

void HTTP_C::RootCertChainRemoveCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x31, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 cert_id{rp.Pop<u32>()};
    auto itr{root_cert_chains.find(context_id)};
    ASSERT(itr != root_cert_chains.end());
    auto cert_itr{std::find_if(itr->second.certificates.begin(), itr->second.certificates.end(),
                               [&](const auto& cert) { return cert.handle == cert_id; })};
    if (cert_itr != itr->second.certificates.end()) {
        itr->second.certificates.erase(cert_itr);
        itr->second.certs_counter--;
    }
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, cert_id={}", context_id, cert_id);
}

void HTTP_C::SetClientCertContext(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x29, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 client_cert_context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto cert_itr{client_certs.find(client_cert_context_id)};
    ASSERT(cert_itr != client_certs.end());
    itr->second.ssl_config.enable_client_cert = true;
    itr->second.ssl_config.client_cert_ctx.cert_id = cert_itr->second.cert_id;
    itr->second.ssl_config.client_cert_ctx.certificate = cert_itr->second.certificate;
    itr->second.ssl_config.client_cert_ctx.handle = cert_itr->second.handle;
    itr->second.ssl_config.client_cert_ctx.private_key = cert_itr->second.private_key;
    itr->second.ssl_config.client_cert_ctx.session_id = session_data->session_id;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_HTTP, "context_id={}, client_cert_context_id={}", context_id,
              client_cert_context_id);
}

void HTTP_C::SetSSLOpt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x2B, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 ssl_options{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.ssl_config.options = ssl_options;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, ssl_options=0x{:X}", context_id, ssl_options);
}

void HTTP_C::SetSSLClearOpt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x2C, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 bitmask{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.ssl_config.options &= ~bitmask;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, bitmask=0x{:X}", context_id, bitmask);
}

void HTTP_C::BeginRequest(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x9, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    if (!itr->second.ssl_config.enable_client_cert && ClCertA.init) {
        itr->second.ssl_config.enable_client_cert = true;
        itr->second.ssl_config.client_cert_ctx.certificate = ClCertA.certificate;
        itr->second.ssl_config.client_cert_ctx.private_key = ClCertA.private_key;
    }
    itr->second.state = RequestState::InProgress;
    itr->second.Send();
    itr->second.state = RequestState::ReadyToDownloadContent;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}", context_id);
}

void HTTP_C::BeginRequestAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x9, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    if (!itr->second.ssl_config.enable_client_cert && ClCertA.init) {
        itr->second.ssl_config.enable_client_cert = true;
        itr->second.ssl_config.client_cert_ctx.certificate = ClCertA.certificate;
        itr->second.ssl_config.client_cert_ctx.private_key = ClCertA.private_key;
    }
    itr->second.state = RequestState::InProgress;
    itr->second.Send();
    itr->second.state = RequestState::ReadyToDownloadContent;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}", context_id);
}

void HTTP_C::ReceiveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0xB, 2, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 buffer_size{rp.Pop<u32>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    const u32 size{
        std::min(buffer_size, itr->second.GetResponseContentLength() - itr->second.current_offset)};
    buffer.Write(&itr->second.response.body[itr->second.current_offset], 0, size);
    itr->second.current_offset += size;
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(itr->second.current_offset < itr->second.GetResponseContentLength()
                ? RESULT_DOWNLOADPENDING
                : RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
    LOG_TRACE(Service_HTTP, "context_id={}, buffer_size={}", context_id, buffer_size);
}

void HTTP_C::ReceiveDataTimeout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0xC, 4, 2};
    const u32 context_id{rp.Pop<u32>()};
    const u32 buffer_size{rp.Pop<u32>()};
    const u64 timeout{rp.Pop<u64>()};
    auto& buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    const u32 size{
        std::min(buffer_size, itr->second.GetResponseContentLength() - itr->second.current_offset)};
    buffer.Write(&itr->second.response.body[itr->second.current_offset], 0, size);
    itr->second.current_offset += size;
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(itr->second.current_offset < itr->second.GetResponseContentLength()
                ? RESULT_DOWNLOADPENDING
                : RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
    LOG_TRACE(Service_HTTP, "context_id={}, buffer_size={}, timeout={}", context_id, buffer_size,
              timeout);
}

void HTTP_C::SetProxyDefault(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0xE, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.proxy_default = true;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}", context_id);
}

void HTTP_C::SetKeepAlive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x37, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const bool keep_alive{rp.Pop<bool>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.SetKeepAlive(keep_alive);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, keep_alive={}", context_id, keep_alive);
}

void HTTP_C::SetSocketBufferSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x10, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 val{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.socket_buffer_size = val;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "val={}", val);
}

void HTTP_C::Finalize(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    session_data->initialized = false;
    session_data->current_http_context.reset();
    shared_memory = nullptr;
    IPC::ResponseBuilder rb{ctx, 0x39, 1, 0};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "called");
}

void HTTP_C::GetSSLError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x2A, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(itr->second.ssl_error);
    LOG_DEBUG(Service_HTTP, "ssl_error={}", itr->second.ssl_error);
}

void HTTP_C::CreateRootCertChain(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    RootCertChain chain;
    chain.session_id = session_data->session_id;
    chain.handle = ++root_cert_chains_counter;
    ++session_data->num_root_cert_chains;
    root_cert_chains.emplace(chain.handle, chain);
    IPC::ResponseBuilder rb{ctx, 0x2D, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(root_cert_chains_counter);
}

void HTTP_C::DestroyRootCertChain(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x2E, 1, 0};
    const u32 context_id{rp.Pop<u32>()};
    ASSERT(root_cert_chains.find(context_id) != root_cert_chains.end());
    root_cert_chains.erase(context_id);
    --root_cert_chains_counter;
    --session_data->num_root_cert_chains;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::RootCertChainAddDefaultCert(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x30, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 cert_id{rp.Pop<u32>()};
    ASSERT(cert_id <= 0xB);
    auto itr{root_cert_chains.find(context_id)};
    ASSERT(itr != root_cert_chains.end());
    RootCertChain::RootCACert cert;
    cert.session_id = session_data->session_id;
    cert.handle = ++itr->second.certs_counter;
    cert.certificate = default_root_certs[cert_id].certificate;
    itr->second.certificates.push_back(cert);
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(itr->second.certs_counter);
}

void HTTP_C::SelectRootCertChain(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x26, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u32 chain_id{rp.Pop<u32>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    auto chain_itr{root_cert_chains.find(chain_id)};
    ASSERT(chain_itr != root_cert_chains.end());
    itr->second.ssl_config.root_ca_chain = chain_itr->second;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::SetClientCert(Kernel::HLERequestContext& ctx) {
    auto session_data{GetSessionData(ctx.Session())};
    ASSERT(session_data);
    IPC::RequestParser rp{ctx, 0x27, 3, 4};
    const u32 context_id{rp.Pop<u32>()};
    const u32 cert_buffer_size{rp.Pop<u32>()};
    const u32 private_key_buffer_size{rp.Pop<u32>()};
    auto& cert_buffer{rp.PopMappedBuffer()};
    auto& private_key_buffer{rp.PopMappedBuffer()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.ssl_config.enable_client_cert = true;
    itr->second.ssl_config.client_cert_ctx.session_id = session_data->session_id;
    cert_buffer.Read(itr->second.ssl_config.client_cert_ctx.certificate.data(), 0,
                     cert_buffer_size);
    private_key_buffer.Read(itr->second.ssl_config.client_cert_ctx.private_key.data(), 0,
                            private_key_buffer_size);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_HTTP, "context_id={}, cert_buffer_size={}, private_key_buffer_size={}",
              context_id, cert_buffer_size, private_key_buffer_size);
}

void HTTP_C::SetClientCertDefault(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x28, 2, 0};
    const u32 context_id{rp.Pop<u32>()};
    const u8 cert_id{rp.Pop<u8>()};
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    if (cert_id != 0x40) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ERROR_WRONG_CERT_ID);
        return;
    }
    if (ClCertA.init) {
        itr->second.ssl_config.enable_client_cert = true;
        itr->second.ssl_config.client_cert_ctx.cert_id = cert_id;
        itr->second.ssl_config.client_cert_ctx.certificate = ClCertA.certificate;
        itr->second.ssl_config.client_cert_ctx.private_key = ClCertA.private_key;
    }
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::SetBasicAuthorization(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0xF, 3, 4};
    const u32 context_id{rp.Pop<u32>()};
    const u32 username_length{rp.Pop<u32>()};
    const u32 password_length{rp.Pop<u32>()};
    const auto username_buffer{rp.PopStaticBuffer()};
    const auto password_buffer{rp.PopStaticBuffer()};
    std::string username(username_length, '\0');
    std::memcpy(&username[0], username_buffer.data(), username_length);
    std::string password(password_length, '\0');
    std::memcpy(&password[0], password_buffer.data(), password_length);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    using namespace CryptoPP;
    using Name::InsertLineBreaks;
    using Name::Pad;
    auto in{fmt::format("{}:{}", username, password)};
    std::string out;
    Base64Encoder encoder;
    auto params{MakeParameters(InsertLineBreaks(), false)(Pad(), false)};
    encoder.IsolatedInitialize(params);
    encoder.Attach(new StringSink(out));
    encoder.Put((byte*)in.data(), in.length());
    encoder.MessageEnd();
    itr->second.headers["Authorization"] = fmt::format("Basic {}", out);
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_HTTP, "(stubbed) username={}, password={}", username, password);
}

void HTTP_C::SetProxy(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0xD, 5, 6};
    const u32 context_id{rp.Pop<u32>()};
    const u32 proxy_length{rp.Pop<u32>()};
    const u16 port{rp.Pop<u16>()};
    const u32 username_length{rp.Pop<u32>()};
    const u32 password_length{rp.Pop<u32>()};
    const auto proxy_buffer{rp.PopStaticBuffer()};
    const auto username_buffer{rp.PopStaticBuffer()};
    const auto password_buffer{rp.PopStaticBuffer()};
    std::string proxy(proxy_length, '\0');
    std::memcpy(&proxy[0], proxy_buffer.data(), proxy_length);
    std::string username(username_length, '\0');
    std::memcpy(&username[0], username_buffer.data(), username_length);
    std::string password(password_length, '\0');
    std::memcpy(&password[0], password_buffer.data(), password_length);
    auto itr{contexts.find(context_id)};
    ASSERT(itr != contexts.end());
    itr->second.proxy = Context::Proxy{};
    itr->second.proxy->url = proxy;
    itr->second.proxy->username = username;
    itr->second.proxy->password = password;
    itr->second.proxy->port = port;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_HTTP, "(stubbed) proxy={}, username={}, password={}, port={}", proxy,
                username, password, username);
}

void HTTP_C::DecryptClCertA(Core::System& system) {
    constexpr u32 iv_length{16};
    FileSys::NCCHArchive archive{system, 0x0004001b00010002, Service::FS::MediaType::NAND};
    std::array<char, 8> exefs_filepath;
    FileSys::Path file_path{FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::RomFS, exefs_filepath)};
    FileSys::Mode open_mode{};
    open_mode.read_flag.Assign(1);
    auto file_result{archive.OpenFile(file_path, open_mode)};
    if (file_result.Failed()) {
        LOG_ERROR(Service_HTTP, "ClCertA file missing");
        return;
    }
    auto romfs{std::move(file_result).Unwrap()};
    std::vector<u8> romfs_buffer(romfs->GetSize());
    romfs->Read(0, romfs_buffer.size(), romfs_buffer.data());
    romfs->Close();
    if (!HW::AES::IsNormalKeyAvailable(HW::AES::KeySlotID::SSLKey)) {
        LOG_ERROR(Service_HTTP, "NormalKey in KeySlot 0x0D missing");
        return;
    }
    HW::AES::AESKey key{HW::AES::GetNormalKey(HW::AES::KeySlotID::SSLKey)};
    const RomFS::RomFSFile cert_file{
        RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-cert.bin"})};
    if (cert_file.Length() == 0) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin missing");
        return;
    }
    if (cert_file.Length() <= iv_length) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin size is too small. Size: {}",
                  cert_file.Length());
        return;
    }
    std::vector<u8> cert_data(cert_file.Length() - iv_length);
    using CryptoPP::AES;
    CryptoPP::CBC_Mode<AES>::Decryption aes_cert;
    std::array<u8, iv_length> cert_iv;
    std::memcpy(cert_iv.data(), cert_file.Data(), iv_length);
    aes_cert.SetKeyWithIV(key.data(), AES::BLOCKSIZE, cert_iv.data());
    aes_cert.ProcessData(cert_data.data(), cert_file.Data() + iv_length,
                         cert_file.Length() - iv_length);
    const RomFS::RomFSFile key_file{RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-key.bin"})};
    if (key_file.Length() == 0) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin missing");
        return;
    }
    if (key_file.Length() <= iv_length) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin size is too small. Size: {}",
                  key_file.Length());
        return;
    }
    std::vector<u8> key_data(key_file.Length() - iv_length);
    CryptoPP::CBC_Mode<AES>::Decryption aes_key;
    std::array<u8, iv_length> key_iv;
    std::memcpy(key_iv.data(), key_file.Data(), iv_length);
    aes_key.SetKeyWithIV(key.data(), AES::BLOCKSIZE, key_iv.data());
    aes_key.ProcessData(key_data.data(), key_file.Data() + iv_length,
                        key_file.Length() - iv_length);
    ClCertA.certificate = std::move(cert_data);
    ClCertA.private_key = std::move(key_data);
    ClCertA.init = true;
}

void HTTP_C::LoadDefaultCerts(Core::System& system) {
    default_root_certs[0].cert_id = 0x1;
    default_root_certs[0].name = "Nintendo CA";
    default_root_certs[0].size = 897;
    default_root_certs[1].cert_id = 0x2;
    default_root_certs[1].name = "Nintendo CA - G2";
    default_root_certs[1].size = 1173;
    default_root_certs[2].cert_id = 0x3;
    default_root_certs[2].name = "Nintendo CA - G3";
    default_root_certs[2].size = 1060;
    default_root_certs[3].cert_id = 0x4;
    default_root_certs[3].name = "Nintendo Class 2 CA";
    default_root_certs[3].size = 924;
    default_root_certs[4].cert_id = 0x5;
    default_root_certs[4].name = "Nintendo Class 2 CA - G2";
    default_root_certs[4].size = 1200;
    default_root_certs[5].cert_id = 0x6;
    default_root_certs[5].name = "Nintendo Class 2 CA - G3";
    default_root_certs[5].size = 1200;
    default_root_certs[6].cert_id = 0x7;
    default_root_certs[6].name = "GTE CyberTrust Global Root";
    default_root_certs[6].size = 606;
    default_root_certs[7].cert_id = 0x8;
    default_root_certs[7].name = "AddTrust External CA Root";
    default_root_certs[7].size = 1086;
    default_root_certs[8].cert_id = 0x9;
    default_root_certs[8].name = "COMODO RSA Certification Authority";
    default_root_certs[8].size = 1514;
    default_root_certs[9].cert_id = 0xA;
    default_root_certs[9].name = "USERTrust RSA Certification Authority";
    default_root_certs[9].size = 1488;
    default_root_certs[10].cert_id = 0xB;
    default_root_certs[10].name = "DigiCert High Assurance EV Root CA";
    default_root_certs[10].size = 969;
    FileSys::NCCHArchive archive{system, 0x0004013000002F02, Service::FS::MediaType::NAND};
    std::array<char, 8> exefs_filepath{".code"};
    FileSys::Path file_path{FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::Code, exefs_filepath)};
    FileSys::Mode open_mode{};
    open_mode.read_flag.Assign(1);
    auto file_result{archive.OpenFile(file_path, open_mode)};
    if (file_result.Failed()) {
        LOG_ERROR(Service_HTTP, "SSL system archive file missing");
        return;
    }
    auto code{std::move(file_result).Unwrap()};
    std::vector<u8> code_buffer(code->GetSize());
    code->Read(0, code_buffer.size(), code_buffer.data());
    code->Close();
    std::string str(code_buffer.begin(), code_buffer.end());
    std::size_t pos{str.find("Nintendo") - 79};
    if (pos == std::string::npos) {
        LOG_ERROR(Service_HTTP, "Couldn't locate start position for certificates in SSL module");
        return;
    }
    for (auto& c : default_root_certs) {
        auto cert{&c};
        if (pos + cert->size > code_buffer.size()) {
            LOG_ERROR(Service_HTTP, "SSL module size too small");
            return;
        }
        cert->certificate = std::vector<u8>(&code_buffer[pos], &code_buffer[pos + cert->size]);
        pos += cert->size;
    }
}

HTTP_C::HTTP_C(Core::System& system) : ServiceFramework{"http:C", 32} {
    static const FunctionInfo functions[]{
        {0x00010044, &HTTP_C::Initialize, "Initialize"},
        {0x00020082, &HTTP_C::CreateContext, "CreateContext"},
        {0x00030040, &HTTP_C::CloseContext, "CloseContext"},
        {0x00040040, nullptr, "CancelConnection"},
        {0x00050040, &HTTP_C::GetRequestState, "GetRequestState"},
        {0x00060040, &HTTP_C::GetDownloadSizeState, "GetDownloadSizeState"},
        {0x00070040, nullptr, "GetRequestError"},
        {0x00080042, &HTTP_C::InitializeConnectionSession, "InitializeConnectionSession"},
        {0x00090040, &HTTP_C::BeginRequest, "BeginRequest"},
        {0x000A0040, &HTTP_C::BeginRequestAsync, "BeginRequestAsync"},
        {0x000B0082, &HTTP_C::ReceiveData, "ReceiveData"},
        {0x000C0102, &HTTP_C::ReceiveDataTimeout, "ReceiveDataTimeout"},
        {0x000D0146, &HTTP_C::SetProxy, "SetProxy"},
        {0x000E0040, &HTTP_C::SetProxyDefault, "SetProxyDefault"},
        {0x000F00C4, &HTTP_C::SetBasicAuthorization, "SetBasicAuthorization"},
        {0x00100080, &HTTP_C::SetSocketBufferSize, "SetSocketBufferSize"},
        {0x001100C4, &HTTP_C::AddRequestHeader, "AddRequestHeader"},
        {0x001200C4, &HTTP_C::AddPostDataAscii, "AddPostDataAscii"},
        {0x001300C4, &HTTP_C::AddPostDataBinary, "AddPostDataBinary"},
        {0x00140082, &HTTP_C::AddPostDataRaw, "AddPostDataRaw"},
        {0x00150080, &HTTP_C::SetPostDataType, "SetPostDataType"},
        {0x001600C4, nullptr, "SendPostDataAscii"},
        {0x00170144, nullptr, "SendPostDataAsciiTimeout"},
        {0x001800C4, nullptr, "SendPostDataBinary"},
        {0x00190144, nullptr, "SendPostDataBinaryTimeout"},
        {0x001A0082, nullptr, "SendPostDataRaw"},
        {0x001B0102, &HTTP_C::SendPOSTDataRawTimeout, "SendPOSTDataRawTimeout"},
        {0x001C0080, nullptr, "SetPostDataEncoding"},
        {0x001D0040, &HTTP_C::NotifyFinishSendPostData, "NotifyFinishSendPostData"},
        {0x001E00C4, &HTTP_C::GetResponseHeader, "GetResponseHeader"},
        {0x001F0144, &HTTP_C::GetResponseHeaderTimeout, "GetResponseHeaderTimeout"},
        {0x00200082, &HTTP_C::GetResponseData, "GetResponseData"},
        {0x00210102, &HTTP_C::GetResponseDataTimeout, "GetResponseDataTimeout"},
        {0x00220040, &HTTP_C::GetResponseStatusCode, "GetResponseStatusCode"},
        {0x002300C0, &HTTP_C::GetResponseStatusCodeTimeout, "GetResponseStatusCodeTimeout"},
        {0x00240082, &HTTP_C::AddTrustedRootCA, "AddTrustedRootCA"},
        {0x00250080, &HTTP_C::AddDefaultCert, "AddDefaultCert"},
        {0x00260080, &HTTP_C::SelectRootCertChain, "SelectRootCertChain"},
        {0x002700C4, &HTTP_C::SetClientCert, "SetClientCert"},
        {0x00280080, &HTTP_C::SetClientCertDefault, "SetClientCertDefault"},
        {0x00290080, &HTTP_C::SetClientCertContext, "SetClientCertContext"},
        {0x002A0040, &HTTP_C::GetSSLError, "GetSSLError"},
        {0x002B0080, &HTTP_C::SetSSLOpt, "SetSSLOpt"},
        {0x002C0080, &HTTP_C::SetSSLClearOpt, "SetSSLClearOpt"},
        {0x002D0000, &HTTP_C::CreateRootCertChain, "CreateRootCertChain"},
        {0x002E0040, &HTTP_C::DestroyRootCertChain, "DestroyRootCertChain"},
        {0x002F0082, &HTTP_C::RootCertChainAddCert, "RootCertChainAddCert"},
        {0x00300080, &HTTP_C::RootCertChainAddDefaultCert, "RootCertChainAddDefaultCert"},
        {0x00310080, &HTTP_C::RootCertChainRemoveCert, "RootCertChainRemoveCert"},
        {0x00320084, &HTTP_C::OpenClientCertContext, "OpenClientCertContext"},
        {0x00330040, &HTTP_C::OpenDefaultClientCertContext, "OpenDefaultClientCertContext"},
        {0x00340040, &HTTP_C::CloseClientCertContext, "CloseClientCertContext"},
        {0x00350186, nullptr, "SetDefaultProxy"},
        {0x00360000, nullptr, "ClearDNSCache"},
        {0x00370080, &HTTP_C::SetKeepAlive, "SetKeepAlive"},
        {0x003800C0, nullptr, "SetPostDataTypeSize"},
        {0x00390000, &HTTP_C::Finalize, "Finalize"},
    };
    RegisterHandlers(functions);
    DecryptClCertA(system);
    LoadDefaultCerts(system);
}

void InstallInterfaces(Core::System& system) {
    std::make_shared<HTTP_C>(system)->InstallAsService(system.ServiceManager());
}

} // namespace Service::HTTP