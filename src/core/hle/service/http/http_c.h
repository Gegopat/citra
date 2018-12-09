// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace httplib {
class Client;
} // namespace httplib

namespace Service::HTTP {

using Headers = std::unordered_map<std::string, std::string>;

struct Response {
    std::string raw, body;
    Headers headers;
    u32 status_code;

    bool HasHeader(const std::string& name) const {
        return headers.find(name) != headers.end();
    }

    std::string GetHeader(const std::string& name) const {
        return headers.find(name)->second;
    }
};

enum class RequestMethod : u8 {
    None = 0x0,
    Get = 0x1,
    Post = 0x2,
    Head = 0x3,
    Put = 0x4,
    Delete = 0x5,
    PostEmpty = 0x6,
    PutEmpty = 0x7,
};

/// The number of request methods, any valid method must be less than this.
constexpr u32 TotalRequestMethods{8};

enum class RequestState : u8 {
    NotStarted = 0x1,             // Request has not started yet.
    InProgress = 0x5,             // Request in progress, sending request over the network.
    ReadyToDownloadContent = 0x7, // Ready to download the content. (needs verification)
    ReadyToDownload = 0x8,        // Ready to download?
    TimedOut = 0xA,               // Request timed out?
};

/// Represents a client certificate along with its private key, stored as a byte array of DER data.
/// There can only be at most one client certificate context attached to an HTTP context at any
/// given time.
struct ClientCertContext {
    using Handle = u32;
    Handle handle;
    u32 session_id;
    u8 cert_id;
    std::vector<u8> certificate;
    std::vector<u8> private_key;
};

/// Represents a root certificate chain, it contains a list of DER-encoded certificates for
/// verifying HTTP requests. An HTTP context can have at most one root certificate chain attached to
/// it, but the chain may contain an arbitrary number of certificates in it.
struct RootCertChain {
    struct RootCACert {
        using Handle = u32;
        Handle handle;
        u32 session_id;
        std::vector<u8> certificate;
    };

    using Handle = u32;
    Handle handle;
    u32 session_id;
    std::vector<RootCACert> certificates;
    u32 certs_counter{};
};

struct DefaultRootCert {
    u8 cert_id;
    std::string name;
    std::vector<u8> certificate;
    std::size_t size;
};

/// Represents an HTTP context.
class Context final {
public:
    using Handle = u32;

    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Context(Context&& other) = default;
    Context& operator=(Context&&) = default;

    struct Proxy {
        std::string url;
        std::string username;
        std::string password;
        u16 port;
    };

    struct BasicAuth {
        std::string username;
        std::string password;
    };

    struct SSLConfig {
        u32 options;
        bool enable_client_cert;
        bool enable_root_cert_chain;
        ClientCertContext client_cert_ctx;
        RootCertChain root_ca_chain;
    };

    struct PostData {
        enum class Type {
            Ascii,
            Binary,
            Raw,
        };

        Type type;

        struct {
            std::string name;
            std::string value;
        } ascii;

        struct {
            std::string name;
            std::vector<u8> data;
        } binary;

        struct {
            std::string data;
        } raw;
    };

    Handle handle;
    u32 session_id;
    std::string path;
    RequestMethod method;
    RequestState state{RequestState::NotStarted};
    std::optional<Proxy> proxy;
    SSLConfig ssl_config{};
    u32 socket_buffer_size;
    Headers headers;
    std::vector<PostData> post_data;
    u32 current_offset{};
    Response response;
    bool proxy_default;
    u32 ssl_error{};
    std::unique_ptr<httplib::Client> client;

    u32 GetResponseContentLength() const;
    void Send();
    void SetKeepAlive(bool);
    std::string GetRawResponseWithoutBody() const;
};

struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
    /// The HTTP context that is currently bound to this session, this can be empty if no context
    /// has been bound. Certain commands can only be called on a session with a bound context.
    std::optional<Context::Handle> current_http_context;

    u32 session_id;

    /// Number of HTTP contexts that are currently opened in this session.
    u32 num_http_contexts{};

    /// Number of ClientCert contexts that are currently opened in this session.
    u32 num_client_certs{};

    /// Number of RootCertChain contexts that are currently opened in this session.
    u32 num_root_cert_chains{};

    /// Whether this session has been initialized in some way, be it via Initialize or
    /// InitializeConnectionSession.
    bool initialized{};
};

class HTTP_C final : public ServiceFramework<HTTP_C, SessionData> {
public:
    explicit HTTP_C(Core::System& system);

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void CreateContext(Kernel::HLERequestContext& ctx);
    void CloseContext(Kernel::HLERequestContext& ctx);
    void GetRequestState(Kernel::HLERequestContext& ctx);
    void GetDownloadSizeState(Kernel::HLERequestContext& ctx);
    void InitializeConnectionSession(Kernel::HLERequestContext& ctx);
    void BeginRequest(Kernel::HLERequestContext& ctx);
    void BeginRequestAsync(Kernel::HLERequestContext& ctx);
    void ReceiveData(Kernel::HLERequestContext& ctx);
    void ReceiveDataTimeout(Kernel::HLERequestContext& ctx);
    void SetProxyDefault(Kernel::HLERequestContext& ctx);
    void SetSocketBufferSize(Kernel::HLERequestContext& ctx);
    void AddRequestHeader(Kernel::HLERequestContext& ctx);
    void AddPostDataAscii(Kernel::HLERequestContext& ctx);
    void AddPostDataBinary(Kernel::HLERequestContext& ctx);
    void AddPostDataRaw(Kernel::HLERequestContext& ctx);
    void GetResponseHeader(Kernel::HLERequestContext& ctx);
    void GetResponseHeaderTimeout(Kernel::HLERequestContext& ctx);
    void GetResponseStatusCode(Kernel::HLERequestContext& ctx);
    void GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx);
    void SetClientCertContext(Kernel::HLERequestContext& ctx);
    void SetSSLOpt(Kernel::HLERequestContext& ctx);
    void SetSSLClearOpt(Kernel::HLERequestContext& ctx);
    void SetKeepAlive(Kernel::HLERequestContext& ctx);
    void OpenClientCertContext(Kernel::HLERequestContext& ctx);
    void OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx);
    void CloseClientCertContext(Kernel::HLERequestContext& ctx);
    void Finalize(Kernel::HLERequestContext& ctx);
    void GetSSLError(Kernel::HLERequestContext& ctx);
    void CreateRootCertChain(Kernel::HLERequestContext& ctx);
    void DestroyRootCertChain(Kernel::HLERequestContext& ctx);
    void RootCertChainAddDefaultCert(Kernel::HLERequestContext& ctx);
    void SelectRootCertChain(Kernel::HLERequestContext& ctx);
    void SetClientCertDefault(Kernel::HLERequestContext& ctx);
    void GetResponseData(Kernel::HLERequestContext& ctx);
    void GetResponseDataTimeout(Kernel::HLERequestContext& ctx);
    void SetPostDataType(Kernel::HLERequestContext& ctx);
    void SendPOSTDataRawTimeout(Kernel::HLERequestContext& ctx);
    void NotifyFinishSendPostData(Kernel::HLERequestContext& ctx);
    void AddTrustedRootCA(Kernel::HLERequestContext& ctx);
    void AddDefaultCert(Kernel::HLERequestContext& ctx);
    void RootCertChainAddCert(Kernel::HLERequestContext& ctx);
    void RootCertChainRemoveCert(Kernel::HLERequestContext& ctx);
    void SetClientCert(Kernel::HLERequestContext& ctx);
    void SetBasicAuthorization(Kernel::HLERequestContext& ctx);
    void SetProxy(Kernel::HLERequestContext& ctx);

    void DecryptClCertA(Core::System& system);
    void LoadDefaultCerts(Core::System& system);

    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;

    /// The next number to use when a new HTTP session is initialized.
    u32 session_counter{};

    /// The next handle number to use when a new HTTP context is created.
    Context::Handle context_counter{};

    /// The next handle number to use when a new ClientCert context is created.
    ClientCertContext::Handle client_certs_counter{};

    /// The next handle number to use when a new RootCertChain context is created.
    RootCertChain::Handle root_cert_chains_counter{};

    /// Global list of HTTP contexts currently opened.
    std::unordered_map<Context::Handle, Context> contexts;

    /// Global list of ClientCert contexts currently opened.
    std::unordered_map<ClientCertContext::Handle, ClientCertContext> client_certs;

    /// Global list of RootCertChain contexts currently opened.
    std::unordered_map<RootCertChain::Handle, RootCertChain> root_cert_chains;

    struct {
        std::vector<u8> certificate;
        std::vector<u8> private_key;
        bool init{};
    } ClCertA;

    std::array<DefaultRootCert, 11> default_root_certs;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::HTTP
