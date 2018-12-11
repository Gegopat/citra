// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"
#include "core/hle/service/am/am_net.h"
#include "core/hle/service/am/am_sys.h"
#include "core/hle/service/am/am_u.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "core/settings.h"

namespace Service::AM {

constexpr u16 PLATFORM_CTR{0x0004};
constexpr u16 CATEGORY_SYSTEM{0x0010};
constexpr u16 CATEGORY_DLP{0x0001};
constexpr u8 VARIATION_SYSTEM{0x02};
constexpr u32 PROGRAM_ID_HIGH_UPDATE{0x0004000E};
constexpr u32 PROGRAM_ID_HIGH_DLC{0x0004008C};

struct TitleInfo {
    u64_le pid;
    u64_le size;
    u16_le version;
    u16_le unused;
    u32_le type;
};

static_assert(sizeof(TitleInfo) == 0x18, "Title info structure size is wrong");

class CIAFile::DecryptionState {
public:
    std::vector<CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption> content;
};

CIAFile::CIAFile(Service::FS::MediaType media_type)
    : media_type{media_type}, decryption_state{std::make_unique<DecryptionState>()} {}

CIAFile::~CIAFile() {
    Close();
}

constexpr u8 OWNERSHIP_DOWNLOADED{0x01};
constexpr u8 OWNERSHIP_OWNED{0x02};

struct ContentInfo {
    u16_le index;
    u16_le type;
    u32_le content_id;
    u64_le size;
    u8 ownership;
    INSERT_PADDING_BYTES(0x7);
};

static_assert(sizeof(ContentInfo) == 0x18, "Content info structure size is wrong");

struct TicketInfo {
    u64_le program_id;
    u64_le ticket_id;
    u16_le version;
    u16_le unused;
    u32_le size;
};

static_assert(sizeof(TicketInfo) == 0x18, "Ticket info structure size is wrong");

ResultVal<std::size_t> CIAFile::Read(u64 offset, std::size_t length, u8* buffer) const {
    UNIMPLEMENTED();
    return MakeResult<std::size_t>(length);
}

ResultCode CIAFile::WriteTicket() {
    container.LoadTicket(data, container.GetTicketOffset());
    install_state = CIAInstallState::TicketLoaded;
    return RESULT_SUCCESS;
}

ResultCode CIAFile::WriteTitleMetadata() {
    container.LoadTitleMetadata(data, container.GetTitleMetadataOffset());
    FileSys::TitleMetadata tmd{container.GetTitleMetadata()};
    tmd.Print();
    // If a TMD already exists for this app (ie 00000000.tmd), the incoming TMD
    // will be the same plus one, (ie 00000001.tmd), both will be kept until
    // the install is finalized and old contents can be discarded.
    if (FileUtil::Exists(GetMetadataPath(media_type, tmd.GetProgramID())))
        is_update = true;
    std::string tmd_path{GetMetadataPath(media_type, tmd.GetProgramID(), is_update)};
    // Create content/ folder if it doesn't exist
    std::string tmd_folder;
    Common::SplitPath(tmd_path, &tmd_folder, nullptr, nullptr);
    FileUtil::CreateFullPath(tmd_folder);
    // Save TMD so that we can start getting new .app paths
    if (tmd.Save(tmd_path) != Loader::ResultStatus::Success)
        return FileSys::ERROR_INSUFFICIENT_SPACE;
    // Create any other .app folders which may not exist yet
    std::string program_folder;
    Common::SplitPath(GetProgramContentPath(media_type, tmd.GetProgramID(),
                                            FileSys::TMDContentIndex::Main, is_update),
                      &program_folder, nullptr, nullptr);
    FileUtil::CreateFullPath(program_folder);
    auto content_count{container.GetTitleMetadata().GetContentCount()};
    content_written.resize(content_count);
    auto title_key{container.GetTicket().GetTitleKey()};
    if (title_key) {
        decryption_state->content.resize(content_count);
        for (std::size_t i{}; i < content_count; ++i) {
            auto ctr{tmd.GetContentCTRByIndex(i)};
            decryption_state->content[i].SetKeyWithIV(title_key->data(), title_key->size(),
                                                      ctr.data());
        }
    }
    install_state = CIAInstallState::TMDLoaded;
    return RESULT_SUCCESS;
}

ResultVal<std::size_t> CIAFile::WriteContentData(u64 offset, std::size_t length, const u8* buffer) {
    // Data isn't being buffered, so we have to keep track of how much of each <ID>.app
    // has been written since we might get a written buffer which contains multiple .app
    // contents or only part of a larger .app's contents.
    u64 offset_max{offset + length};
    for (int i{}; i < container.GetTitleMetadata().GetContentCount(); i++) {
        if (content_written[i] < container.GetContentSize(i)) {
            // The size, minimum unwritten offset, and maximum unwritten offset of this content
            u64 size{container.GetContentSize(i)};
            u64 range_min{container.GetContentOffset(i) + content_written[i]};
            u64 range_max{container.GetContentOffset(i) + size};
            // The unwritten range for this content is beyond the buffered data we have
            // or comes before the buffered data we have, so skip this content ID.
            if (range_min > offset_max || range_max < offset)
                continue;
            // Figure out how much of this content ID we have just received/can write out
            u64 available_to_write{std::min(offset_max, range_max) - range_min};
            // Since the incoming TMD has already been written, we can use GetProgramContentPath
            // to get the content paths to write to.
            FileSys::TitleMetadata tmd{container.GetTitleMetadata()};
            FileUtil::IOFile file{
                GetProgramContentPath(media_type, tmd.GetProgramID(), i, is_update),
                content_written[i] ? "ab" : "wb"};
            if (!file.IsOpen())
                return FileSys::ERROR_INSUFFICIENT_SPACE;
            std::vector<u8> temp(buffer + (range_min - offset),
                                 buffer + (range_min - offset) + available_to_write);
            if (tmd.GetContentTypeByIndex(static_cast<u16>(i)) &
                FileSys::TMDContentTypeFlag::Encrypted) {
                decryption_state->content[i].ProcessData(temp.data(), temp.data(), temp.size());
            }
            file.WriteBytes(temp.data(), temp.size());
            // Keep tabs on how much of this content ID has been written so new range_min
            // values can be calculated.
            content_written[i] += available_to_write;
            LOG_DEBUG(Service_AM, "Wrote {:x} to content {}, total {:x}", available_to_write, i,
                      content_written[i]);
        }
    }
    return MakeResult<std::size_t>(length);
}

ResultVal<std::size_t> CIAFile::Write(u64 offset, std::size_t length, bool flush,
                                      const u8* buffer) {
    written += length;
    // TODO: Can we assume that things will only be written in sequence?
    // Does AM send an error if we write to things out of order?
    // Or does it just ignore offsets and assume a set sequence of incoming data?
    // The data in CIAs is always stored CIA Header > Cert > Ticket > TMD > Content > Meta.
    // The CIA Header describes Cert, Ticket, TMD, total content sizes, and TMD is needed for
    // content sizes so it ends up becoming a problem of keeping track of how much has been
    // written and what we have been able to pick up.
    if (install_state == CIAInstallState::InstallStarted) {
        std::size_t buf_copy_size{std::min(length, FileSys::CIA_HEADER_SIZE)};
        std::size_t buf_max_size{
            std::min(static_cast<std::size_t>(offset + length), FileSys::CIA_HEADER_SIZE)};
        data.resize(buf_max_size);
        std::memcpy(data.data() + offset, buffer, buf_copy_size);
        // We have enough data to load a CIA header and parse it.
        if (written >= FileSys::CIA_HEADER_SIZE) {
            container.LoadHeader(data);
            container.Print();
            install_state = CIAInstallState::HeaderLoaded;
        }
    }
    // If we don't have a header yet, we can't pull offsets of other sections
    if (install_state == CIAInstallState::InstallStarted)
        return MakeResult<std::size_t>(length);
    // If we have been given data before (or including) .app content, pull it into
    // our buffer, but only pull *up to* the content offset, no further.
    if (offset < container.GetContentOffset()) {
        std::size_t buf_loaded{data.size()};
        std::size_t copy_offset{std::max(static_cast<std::size_t>(offset), buf_loaded)};
        std::size_t buf_offset{buf_loaded - offset};
        std::size_t buf_copy_size{
            std::min(length, static_cast<std::size_t>(container.GetContentOffset() - offset)) -
            buf_loaded};
        std::size_t buf_max_size{std::min(offset + length, container.GetContentOffset())};
        data.resize(buf_max_size);
        std::memcpy(data.data() + copy_offset, buffer + buf_offset, buf_copy_size);
    }
    // TODO: Write out .tik files to nand?
    // The end of our TMD is at the beginning of Content data, so ensure we have that much
    // buffered before trying to parse.
    if (written >= container.GetContentOffset() && install_state != CIAInstallState::TMDLoaded) {
        auto result{WriteTicket()};
        if (result.IsError())
            return result;
        result = WriteTitleMetadata();
        if (result.IsError())
            return result;
    }
    // Content data sizes can only be retrieved from TMD data
    if (install_state != CIAInstallState::TMDLoaded)
        return MakeResult<std::size_t>(length);
    // From this point forward, data will no longer be buffered in data
    auto result{WriteContentData(offset, length, buffer)};
    if (result.Failed())
        return result;
    return MakeResult<std::size_t>(length);
}

u64 CIAFile::GetSize() const {
    return written;
}

bool CIAFile::SetSize(u64 size) const {
    return false;
}

bool CIAFile::Close() const {
    bool complete{true};
    for (std::size_t i{}; i < container.GetTitleMetadata().GetContentCount(); i++) {
        if (content_written[i] < container.GetContentSize(static_cast<u16>(i)))
            complete = false;
    }
    // Install aborted
    if (!complete) {
        LOG_ERROR(Service_AM, "CIAFile closed prematurely, aborting install...");
        FileUtil::DeleteDir(
            GetProgramPath(media_type, container.GetTitleMetadata().GetProgramID()));
        return true;
    }
    // Clean up older content data if we installed newer content on top
    std::string old_tmd_path{
        GetMetadataPath(media_type, container.GetTitleMetadata().GetProgramID(), false)};
    std::string new_tmd_path{
        GetMetadataPath(media_type, container.GetTitleMetadata().GetProgramID(), true)};
    if (FileUtil::Exists(new_tmd_path) && old_tmd_path != new_tmd_path) {
        FileSys::TitleMetadata old_tmd;
        FileSys::TitleMetadata new_tmd;
        old_tmd.Load(old_tmd_path);
        new_tmd.Load(new_tmd_path);
        // For each content ID in the old TMD, check if there is a matching ID in the new
        // TMD. If a CIA contains (and wrote to) an identical ID, it should be kept while
        // IDs which only existed for the old TMD should be deleted.
        for (u16 old_index{}; old_index < old_tmd.GetContentCount(); old_index++) {
            bool abort{};
            for (u16 new_index{}; new_index < new_tmd.GetContentCount(); new_index++) {
                if (old_tmd.GetContentIDByIndex(old_index) ==
                    new_tmd.GetContentIDByIndex(new_index)) {
                    abort = true;
                }
            }
            if (abort)
                break;
            FileUtil::Delete(GetProgramContentPath(media_type, old_tmd.GetProgramID(), old_index));
        }
        FileUtil::Delete(old_tmd_path);
    }
    return true;
}

void CIAFile::Flush() const {}

InstallStatus InstallCIA(const std::string& path,
                         std::function<ProgressCallback>&& update_callback) {
    LOG_INFO(Service_AM, "Installing {}...", path);
    if (!FileUtil::Exists(path)) {
        LOG_ERROR(Service_AM, "File {} doesn't exist!", path);
        return InstallStatus::ErrorFileNotFound;
    }
    FileSys::CIAContainer container;
    if (container.Load(path) == Loader::ResultStatus::Success) {
        Service::AM::CIAFile installFile{
            Service::AM::GetProgramMediaType(container.GetTitleMetadata().GetProgramID())};
        bool title_key_available{container.GetTicket().GetTitleKey().has_value()};
        for (std::size_t i{}; i < container.GetTitleMetadata().GetContentCount(); i++) {
            if ((container.GetTitleMetadata().GetContentTypeByIndex(static_cast<u16>(i)) &
                 FileSys::TMDContentTypeFlag::Encrypted) &&
                !title_key_available) {
                LOG_ERROR(Service_AM, "File {} is encrypted! Aborting...", path);
                return InstallStatus::ErrorEncrypted;
            }
        }
        FileUtil::IOFile file{path, "rb"};
        if (!file.IsOpen())
            return InstallStatus::ErrorFailedToOpenFile;
        std::array<u8, 0x10000> buffer;
        std::size_t total_bytes_read{};
        while (total_bytes_read != file.GetSize()) {
            std::size_t bytes_read{file.ReadBytes(buffer.data(), buffer.size())};
            auto result{installFile.Write(static_cast<u64>(total_bytes_read), bytes_read, true,
                                          static_cast<u8*>(buffer.data()))};
            if (update_callback)
                update_callback(total_bytes_read, file.GetSize());
            if (result.Failed()) {
                LOG_ERROR(Service_AM, "CIA file installation aborted with error code {:08x}",
                          result.Code().raw);
                return InstallStatus::ErrorAborted;
            }
            total_bytes_read += bytes_read;
        }
        installFile.Close();
        LOG_INFO(Service_AM, "Installed {} successfully.", path);
        return InstallStatus::Success;
    }

    LOG_ERROR(Service_AM, "CIA file {} is invalid!", path);
    return InstallStatus::ErrorInvalid;
}

Service::FS::MediaType GetProgramMediaType(u64 program_id) {
    u16 platform{static_cast<u16>(program_id >> 48)};
    u16 category{static_cast<u16>((program_id >> 32) & 0xFFFF)};
    u8 variation{static_cast<u8>(program_id & 0xFF)};
    if (platform != PLATFORM_CTR)
        return Service::FS::MediaType::NAND;
    if (category & CATEGORY_SYSTEM || category & CATEGORY_DLP || variation & VARIATION_SYSTEM)
        return Service::FS::MediaType::NAND;
    return Service::FS::MediaType::SDMC;
}

std::string GetMetadataPath(Service::FS::MediaType media_type, u64 pid, bool update) {
    std::string content_path{GetProgramPath(media_type, pid) + "content/"};
    if (media_type == Service::FS::MediaType::GameCard) {
        LOG_ERROR(Service_AM, "Invalid request for nonexistent gamecard title metadata!");
        return "";
    }
    // The TMD ID is usually held in the title databases, which we don't implement.
    // For now, just scan for any .tmd files which exist, the smallest will be the
    // base ID and the largest will be the (currently installing) update ID.
    constexpr u32 MAX_TMD_ID{0xFFFFFFFF};
    u32 base_id{MAX_TMD_ID};
    u32 update_id{};
    FileUtil::FSTEntry entries{};
    FileUtil::ScanDirectoryTree(content_path, entries);
    for (const FileUtil::FSTEntry& entry : entries.children) {
        std::string filename_filename, filename_extension;
        Common::SplitPath(entry.virtualName, nullptr, &filename_filename, &filename_extension);
        if (filename_extension == ".tmd") {
            u32 id{static_cast<u32>(std::stoul(filename_filename.c_str(), nullptr, 16))};
            base_id = std::min(base_id, id);
            update_id = std::max(update_id, id);
        }
    }
    // If we didn't find anything, default to 00000000.tmd for it to be created.
    if (base_id == MAX_TMD_ID)
        base_id = 0;
    // Update ID should be one more than the last, if it hasn't been created yet.
    if (base_id == update_id)
        update_id++;
    return content_path + fmt::format("{:08x}.tmd", (update ? update_id : base_id));
}

std::string GetProgramContentPath(Service::FS::MediaType media_type, u64 pid, u16 index,
                                  bool update) {
    std::string content_path{GetProgramPath(media_type, pid) + "content/"};
    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO: get current app file if Program ID matches?
        LOG_ERROR(Service_AM, "Request for gamecard partition {} content path unimplemented!",
                  static_cast<u32>(index));
        return "";
    }
    std::string tmd_path{GetMetadataPath(media_type, pid, update)};
    u32 content_id{};
    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        if (index < tmd.GetContentCount())
            content_id = tmd.GetContentIDByIndex(index);
        else {
            LOG_ERROR(Service_AM, "Attempted to get path for non-existent content index {:04x}.",
                      index);
            return "";
        }
        // TODO: how does DLC actually get this folder on hardware?
        // For now, check if the second (index 1) content has the optional flag set, for most
        // programs this is usually the manual and not set optional, DLC has it set optional.
        // All .apps (including index 0) will be in the 00000000/ folder for DLC.
        if (tmd.GetContentCount() > 1 &&
            tmd.GetContentTypeByIndex(1) & FileSys::TMDContentTypeFlag::Optional) {
            content_path += "00000000/";
        }
    }
    return fmt::format("{}{:08x}.app", content_path, content_id);
}

std::string GetProgramPath(Service::FS::MediaType media_type, u64 pid) {
    u32 high{static_cast<u32>(pid >> 32)};
    u32 low{static_cast<u32>(pid & 0xFFFFFFFF)};
    if (media_type == Service::FS::MediaType::NAND || media_type == Service::FS::MediaType::SDMC)
        return fmt::format("{}{:08x}/{:08x}/", GetMediaProgramPath(media_type), high, low);
    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO: get current app path if Program ID matches?
        LOG_ERROR(Service_AM, "Request for gamecard title path unimplemented!");
        return "";
    }
    return "";
}

std::string GetMediaProgramPath(Service::FS::MediaType media_type) {
    if (media_type == Service::FS::MediaType::NAND)
        return fmt::format(
            "{}{}/title/",
            FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir),
            SYSTEM_CID);
    if (media_type == Service::FS::MediaType::SDMC)
        return fmt::format(
            "{}Nintendo 3DS/{}/{}/title/",
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir),
            SYSTEM_CID, SDCARD_CID);
    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO: get current app parent folder if Program ID matches?
        LOG_ERROR(Service_AM, "Request for gamecard parent path unimplemented!");
        return "";
    }
    return "";
}

void Module::ScanForPrograms(Service::FS::MediaType media_type) {
    am_title_list[static_cast<u32>(media_type)].clear();
    std::string title_path{GetMediaProgramPath(media_type)};
    FileUtil::FSTEntry parent_entry{};
    FileUtil::ScanDirectoryTree(title_path, parent_entry, 1);
    for (const FileUtil::FSTEntry& program_id_high : parent_entry.children) {
        for (const FileUtil::FSTEntry& program_id_low : program_id_high.children) {
            std::string program_id_string{program_id_high.virtualName + program_id_low.virtualName};
            if (program_id_string.length() == program_id_VALID_LENGTH) {
                u64 pid{std::stoull(program_id_string.c_str(), nullptr, 16)};
                FileSys::NCCHContainer container{GetProgramContentPath(media_type, pid)};
                if (container.Load() == Loader::ResultStatus::Success)
                    am_title_list[static_cast<u32>(media_type)].push_back(pid);
            }
        }
    }
}

void Module::ScanForAllPrograms() {
    ScanForPrograms(Service::FS::MediaType::NAND);
    ScanForPrograms(Service::FS::MediaType::SDMC);
}

Module::Interface::Interface(std::shared_ptr<Module> am, const char* name, u32 max_session)
    : ServiceFramework{name, max_session}, am{std::move(am)} {}

Module::Interface::~Interface() = default;

std::shared_ptr<Module> Module::Interface::GetModule() {
    return am;
}

void Module::Interface::GetNumPrograms(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0001, 1, 0};
    u32 media_type{rp.Pop<u8>()};
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(static_cast<u32>(am->am_title_list[media_type].size()));
}

void Module::Interface::FindDLCContentInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1002, 4, 4};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    u64 program_id{rp.Pop<u64>()};
    u32 content_count{rp.Pop<u32>()};
    auto& content_requested_in{rp.PopMappedBuffer()};
    auto& content_info_out{rp.PopMappedBuffer()};
    // Validate that only DLC TIDs are passed in
    u32 program_id_high{static_cast<u32>(program_id >> 32)};
    if (program_id_high != PROGRAM_ID_HIGH_DLC) {
        auto rb{rp.MakeBuilder(1, 4)};
        rb.Push(ResultCode(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Usage));
        rb.PushMappedBuffer(content_requested_in);
        rb.PushMappedBuffer(content_info_out);
        return;
    }
    std::vector<u16_le> content_requested(content_count);
    content_requested_in.Read(content_requested.data(), 0, content_count * sizeof(u16));
    std::string tmd_path{GetMetadataPath(media_type, program_id)};
    u32 content_read{};
    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        std::size_t write_offset{};
        // Get info for each content index requested
        for (std::size_t i{}; i < content_count; i++) {
            if (content_requested[i] >= tmd.GetContentCount()) {
                LOG_ERROR(Service_AM,
                          "Attempted to get info for non-existent content index {:04x}.",
                          content_requested[i]);
                auto rb{rp.MakeBuilder(1, 4)};
                rb.Push<u32>(-1); // TODO: Find the right error code
                rb.PushMappedBuffer(content_requested_in);
                rb.PushMappedBuffer(content_info_out);
                return;
            }
            ContentInfo content_info{};
            content_info.index = content_requested[i];
            content_info.type = tmd.GetContentTypeByIndex(content_requested[i]);
            content_info.content_id = tmd.GetContentIDByIndex(content_requested[i]);
            content_info.size = tmd.GetContentSizeByIndex(content_requested[i]);
            content_info.ownership = OWNERSHIP_OWNED; // TODO: Pull this from the ticket.
            if (FileUtil::Exists(
                    GetProgramContentPath(media_type, program_id, content_requested[i])))
                content_info.ownership |= OWNERSHIP_DOWNLOADED;
            content_info_out.Write(&content_info, write_offset, sizeof(ContentInfo));
            write_offset += sizeof(ContentInfo);
            content_read++;
        }
    }
    auto rb{rp.MakeBuilder(1, 4)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(content_requested_in);
    rb.PushMappedBuffer(content_info_out);
}

void Module::Interface::ListDLCContentInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1003, 5, 2};
    u32 content_count{rp.Pop<u32>()};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    u64 program_id{rp.Pop<u64>()};
    u32 start_index{rp.Pop<u32>()};
    auto& content_info_out{rp.PopMappedBuffer()};
    // Validate that only DLC Program IDs are passed in
    u32 program_id_high{static_cast<u32>(program_id >> 32)};
    if (program_id_high != PROGRAM_ID_HIGH_DLC) {
        auto rb{rp.MakeBuilder(2, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Usage));
        rb.Push<u32>(0);
        rb.PushMappedBuffer(content_info_out);
        return;
    }
    auto tmd_path{GetMetadataPath(media_type, program_id)};
    u32 copied{};
    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        u32 end_index{
            std::min(start_index + content_count, static_cast<u32>(tmd.GetContentCount()))};
        std::size_t write_offset{};
        for (u32 i{start_index}; i < end_index; i++) {
            ContentInfo content_info{};
            content_info.index = static_cast<u16>(i);
            content_info.type = tmd.GetContentTypeByIndex(i);
            content_info.content_id = tmd.GetContentIDByIndex(i);
            content_info.size = tmd.GetContentSizeByIndex(i);
            content_info.ownership = OWNERSHIP_OWNED; // TODO: Pull this from the ticket.
            if (FileUtil::Exists(GetProgramContentPath(media_type, program_id, i)))
                content_info.ownership |= OWNERSHIP_DOWNLOADED;
            content_info_out.Write(&content_info, write_offset, sizeof(ContentInfo));
            write_offset += sizeof(ContentInfo);
            copied++;
        }
    }
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(copied);
    rb.PushMappedBuffer(content_info_out);
}

void Module::Interface::DeleteContents(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1004, 4, 2};
    u8 media_type{rp.Pop<u8>()};
    u64 program_id{rp.Pop<u64>()};
    u32 content_count{rp.Pop<u32>()};
    auto& content_ids_in{rp.PopMappedBuffer()};
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(content_ids_in);
    LOG_WARNING(Service_AM, "(stubbed) media_type={}, program_id=0x{:016x}, content_count={}",
                media_type, program_id, content_count);
}

void Module::Interface::GetProgramList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0002, 2, 2};
    u32 count{rp.Pop<u32>()};
    u8 media_type{rp.Pop<u8>()};
    auto& program_id_s_output{rp.PopMappedBuffer()};
    if (media_type > 2) {
        auto rb{rp.MakeBuilder(2, 2)};
        rb.Push<u32>(-1); // TODO: Find the right error code
        rb.Push<u32>(0);
        rb.PushMappedBuffer(program_id_s_output);
        return;
    }
    u32 media_count{static_cast<u32>(am->am_title_list[media_type].size())};
    u32 copied{std::min(media_count, count)};
    program_id_s_output.Write(am->am_title_list[media_type].data(), 0, copied * sizeof(u64));
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(copied);
    rb.PushMappedBuffer(program_id_s_output);
}

ResultCode GetTitleInfoFromList(const std::vector<u64>& program_id_list,
                                Service::FS::MediaType media_type,
                                Kernel::MappedBuffer& title_info_out) {
    std::size_t write_offset{};
    for (u32 i{}; i < program_id_list.size(); i++) {
        std::string tmd_path{GetMetadataPath(media_type, program_id_list[i])};
        TitleInfo title_info{};
        title_info.pid = program_id_list[i];
        FileSys::TitleMetadata tmd;
        if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
            // TODO: This is the total size of all files this process owns,
            // including savefiles and other content. This comes close but is off.
            title_info.size = tmd.GetContentSizeByIndex(FileSys::TMDContentIndex::Main);
            title_info.version = tmd.GetTitleVersion();
            title_info.type = tmd.GetTitleType();
        } else
            return ResultCode(ErrorDescription::NotFound, ErrorModule::AM,
                              ErrorSummary::InvalidState, ErrorLevel::Permanent);
        title_info_out.Write(&title_info, write_offset, sizeof(TitleInfo));
        write_offset += sizeof(TitleInfo);
    }
    return RESULT_SUCCESS;
}

void Module::Interface::GetProgramInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0003, 2, 4};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    u32 title_count{rp.Pop<u32>()};
    auto& program_id_list_buffer{rp.PopMappedBuffer()};
    auto& title_info_out{rp.PopMappedBuffer()};
    std::vector<u64> program_id_list(title_count);
    program_id_list_buffer.Read(program_id_list.data(), 0, title_count * sizeof(u64));
    auto result{GetTitleInfoFromList(program_id_list, media_type, title_info_out)};
    auto rb{rp.MakeBuilder(1, 4)};
    rb.Push(result);
    rb.PushMappedBuffer(program_id_list_buffer);
    rb.PushMappedBuffer(title_info_out);
}

void Module::Interface::DeleteUserProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0004, 3, 0};
    auto media_type{rp.PopEnum<FS::MediaType>()};
    u64 program_id{rp.Pop<u64>()};
    auto rb{rp.MakeBuilder(1, 0)};
    u16 category{static_cast<u16>((program_id >> 32) & 0xFFFF)};
    u8 variation{static_cast<u8>(program_id & 0xFF)};
    if (category & CATEGORY_SYSTEM || category & CATEGORY_DLP || variation & VARIATION_SYSTEM) {
        LOG_ERROR(Service_AM, "Trying to uninstall system program");
        rb.Push(ResultCode(ErrCodes::TryingToUninstallSystemProgram, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Usage));
        return;
    }
    LOG_INFO(Service_AM, "Deleting program 0x{:016x}", program_id);
    auto path{GetProgramPath(media_type, program_id)};
    if (!FileUtil::Exists(path)) {
        rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        LOG_ERROR(Service_AM, "Program not found");
        return;
    }
    bool success{FileUtil::DeleteDirRecursively(path)};
    if (success)
        am->ScanForAllPrograms();
    else
        LOG_ERROR(Service_AM, "DeleteDirRecursively failed: {}", GetLastErrorMsg());
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetProductCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0005, 3, 0};
    FS::MediaType media_type{rp.PopEnum<FS::MediaType>()};
    u64 program_id{rp.Pop<u64>()};
    auto path{GetProgramContentPath(media_type, program_id)};
    if (!FileUtil::Exists(path)) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
    } else {
        auto rb{rp.MakeBuilder(6, 0)};
        FileSys::NCCHContainer ncch{path};
        ncch.Load();
        std::array<u8, 0x10> product_code;
        std::memcpy(product_code.data(), &ncch.ncch_header.product_code,
                    sizeof(ncch.ncch_header.product_code));
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(product_code);
    }
}

void Module::Interface::GetDLCTitleInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1005, 2, 4};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    u32 title_count{rp.Pop<u32>()};
    auto& program_id_list_buffer{rp.PopMappedBuffer()};
    auto& title_info_out{rp.PopMappedBuffer()};
    std::vector<u64> program_id_list(title_count);
    program_id_list_buffer.Read(program_id_list.data(), 0, title_count * sizeof(u64));
    auto result{RESULT_SUCCESS};
    // Validate that DLC TIDs were passed in
    for (u32 i{}; i < title_count; i++) {
        u32 program_id_high{static_cast<u32>(program_id_list[i] >> 32)};
        if (program_id_high != PROGRAM_ID_HIGH_DLC) {
            result = ResultCode(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                                ErrorSummary::InvalidArgument, ErrorLevel::Usage);
            break;
        }
    }
    if (result.IsSuccess())
        result = GetTitleInfoFromList(program_id_list, media_type, title_info_out);
    auto rb{rp.MakeBuilder(1, 4)};
    rb.Push(result);
    rb.PushMappedBuffer(program_id_list_buffer);
    rb.PushMappedBuffer(title_info_out);
}

void Module::Interface::GetPatchTitleInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x100D, 2, 4};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    u32 title_count{rp.Pop<u32>()};
    auto& program_id_list_buffer{rp.PopMappedBuffer()};
    auto& title_info_out{rp.PopMappedBuffer()};
    std::vector<u64> program_id_list(title_count);
    program_id_list_buffer.Read(program_id_list.data(), 0, title_count * sizeof(u64));
    auto result{RESULT_SUCCESS};
    // Validate that update TIDs were passed in
    for (u32 i{}; i < title_count; i++) {
        u32 program_id_high = static_cast<u32>(program_id_list[i] >> 32);
        if (program_id_high != PROGRAM_ID_HIGH_UPDATE) {
            result = ResultCode(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                                ErrorSummary::InvalidArgument, ErrorLevel::Usage);
            break;
        }
    }
    if (result.IsSuccess())
        result = GetTitleInfoFromList(program_id_list, media_type, title_info_out);
    auto rb{rp.MakeBuilder(1, 4)};
    rb.Push(result);
    rb.PushMappedBuffer(program_id_list_buffer);
    rb.PushMappedBuffer(title_info_out);
}

void Module::Interface::ListDataTitleTicketInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1007, 4, 2};
    u32 ticket_count{rp.Pop<u32>()};
    u64 program_id{rp.Pop<u64>()};
    u32 start_index{rp.Pop<u32>()};
    auto& ticket_info_out{rp.PopMappedBuffer()};
    std::size_t write_offset{};
    for (u32 i{}; i < ticket_count; i++) {
        TicketInfo ticket_info{};
        ticket_info.program_id = program_id;
        ticket_info.version = 0; // TODO
        ticket_info.size = 0;    // TODO
        ticket_info_out.Write(&ticket_info, write_offset, sizeof(TicketInfo));
        write_offset += sizeof(TicketInfo);
    }
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(ticket_count);
    rb.PushMappedBuffer(ticket_info_out);
    LOG_WARNING(Service_AM,
                "(stubbed) ticket_count=0x{:08X}, program_id=0x{:016x}, start_index=0x{:08X}",
                ticket_count, program_id, start_index);
}

void Module::Interface::GetDLCContentInfoCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x1001, 3, 0};
    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 program_id{rp.Pop<u64>()};
    // Validate that only DLC TIDs are passed in
    u32 program_id_high{static_cast<u32>(program_id >> 32)};
    if (program_id_high != PROGRAM_ID_HIGH_DLC) {
        auto rb{rp.MakeBuilder(2, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidTID, ErrorModule::AM, ErrorSummary::InvalidArgument,
                           ErrorLevel::Usage));
        rb.Push<u32>(0);
        return;
    }
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS); // No error
    std::string tmd_path{GetMetadataPath(media_type, program_id)};
    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success)
        rb.Push<u32>(static_cast<u32>(tmd.GetContentCount()));
    else {
        rb.Push<u32>(1); // Number of content infos plus one
        LOG_WARNING(Service_AM, "(stubbed) media_type={}, program_id=0x{:016x}",
                    static_cast<u32>(media_type), program_id);
    }
}

void Module::Interface::DeleteTicket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0007, 2, 0};
    u64 program_id{rp.Pop<u64>()};
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AM, "(stubbed) program_id=0x{:016x}", program_id);
}

void Module::Interface::GetNumTickets(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x0008, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    LOG_WARNING(Service_AM, "stubbed");
}

void Module::Interface::GetTicketList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0009, 2, 2};
    u32 ticket_list_count{rp.Pop<u32>()};
    u32 ticket_index{rp.Pop<u32>()};
    auto& ticket_program_ids_out{rp.PopMappedBuffer()};
    auto rb{rp.MakeBuilder(2, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(ticket_list_count);
    rb.PushMappedBuffer(ticket_program_ids_out);
    LOG_WARNING(Service_AM, "(stubbed) ticket_list_count=0x{:08x}, ticket_index=0x{:08x}",
                ticket_list_count, ticket_index);
}

void Module::Interface::QueryAvailableTitleDatabase(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0019, 1, 0};
    u8 media_type{rp.Pop<u8>()};
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(true);
    LOG_WARNING(Service_AM, "(stubbed) media_type={}", media_type);
}

void Module::Interface::CheckContentRights(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0025, 3, 0};
    u64 pid{rp.Pop<u64>()};
    u16 content_index{rp.Pop<u16>()};
    // TODO: Read tickets for this instead?
    bool has_rights{
        FileUtil::Exists(GetProgramContentPath(Service::FS::MediaType::SDMC, pid, content_index))};
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(has_rights);
    LOG_WARNING(Service_AM, "(stubbed) pid={:016x}, content_index={}", pid, content_index);
}

void Module::Interface::CheckContentRightsIgnorePlatform(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x002D, 3, 0};
    u64 pid{rp.Pop<u64>()};
    u16 content_index{rp.Pop<u16>()};
    // TODO: Read tickets for this instead?
    bool has_rights{
        FileUtil::Exists(GetProgramContentPath(Service::FS::MediaType::SDMC, pid, content_index))};
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(has_rights);
    LOG_WARNING(Service_AM, "(stubbed) pid={:016x}, content_index={}", pid, content_index);
}

void Module::Interface::BeginImportProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0402, 1, 0};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    if (am->cia_installing) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::CIACurrentlyInstalling, ErrorModule::AM,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        return;
    }
    // Create our CIAFile handle for the app to write to, and while the app writes
    // Citra will store contents out to sdmc/nand
    const FileSys::Path cia_path{};
    auto file{std::make_shared<Service::FS::File>(am->system, std::make_unique<CIAFile>(media_type),
                                                  cia_path)};

    am->cia_installing = true;
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS); // No error
    rb.PushCopyObjects(file->Connect());
    LOG_WARNING(Service_AM, "(stubbed) media_type={}", static_cast<u32>(media_type));
}

void Module::Interface::BeginImportProgramTemporarily(Kernel::HLERequestContext& ctx) {
    if (am->cia_installing) {
        IPC::ResponseBuilder rb{ctx, 0x0403, 1, 0};
        rb.Push(ResultCode(ErrCodes::CIACurrentlyInstalling, ErrorModule::AM,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        return;
    }
    // Note: This function should register the title in the temp_i.db database, but we can get away
    // with not doing that because we traverse the file system to detect installed programs.
    // Create our CIAFile handle for the app to write to, and while the app writes Citra will store
    // contents out to sdmc/nand
    const FileSys::Path cia_path{};
    auto file{std::make_shared<Service::FS::File>(
        am->system, std::make_unique<CIAFile>(FS::MediaType::NAND), cia_path)};

    am->cia_installing = true;
    IPC::ResponseBuilder rb{ctx, 0x0403, 1, 2};
    rb.Push(RESULT_SUCCESS); // No error
    rb.PushCopyObjects(file->Connect());
    LOG_WARNING(Service_AM, "stubbed");
}

void Module::Interface::EndImportProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0405, 0, 2};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    am->cia_installing = false;
    am->ScanForAllPrograms();
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::EndImportProgramWithoutCommit(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0406, 0, 2};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    // Note: This function is basically a no-op for us since we don't use title.db or ticket.db
    // files to keep track of installed programs.
    am->cia_installing = false;
    am->ScanForAllPrograms();
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::CommitImportPrograms(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0407, 3, 2};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    u32 title_count{rp.Pop<u32>()};
    u8 database{rp.Pop<u8>()};
    auto& buffer{rp.PopMappedBuffer()};
    // Note: This function is basically a no-op for us since we don't use title.db or ticket.db
    // files to keep track of installed programs.
    am->ScanForAllPrograms();
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
}

/// Wraps all File operations to allow adding an offset to them.
class AMFileWrapper : public FileSys::FileBackend {
public:
    AMFileWrapper(std::shared_ptr<Service::FS::File> file, std::size_t offset, std::size_t size)
        : file{std::move(file)}, file_offset{offset}, file_size{size} {}

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override {
        return file->backend->Read(offset + file_offset, length, buffer);
    }

    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                 const u8* buffer) override {
        return file->backend->Write(offset + file_offset, length, flush, buffer);
    }

    u64 GetSize() const override {
        return file_size;
    }

    bool SetSize(u64 size) const override {
        return false;
    }

    bool Close() const override {
        return false;
    }

    void Flush() const override {}

private:
    std::shared_ptr<Service::FS::File> file;
    std::size_t file_offset;
    std::size_t file_size;
};

ResultVal<std::unique_ptr<AMFileWrapper>> GetFileFromSession(
    Kernel::SharedPtr<Kernel::ClientSession> file_session) {
    // Step up the chain from ClientSession->ServerSession and then
    // cast to File. For AM on a real console, invalid handles actually hang the system.
    if (!file_session->parent)
        // Invalid handle. Emulate the hang
        for (;;)
            ;
    Kernel::SharedPtr<Kernel::ServerSession> server{file_session->parent->server};
    if (!server) {
        LOG_WARNING(Service_AM, "File handle ServerSession disconnected!");
        return Kernel::ERR_SESSION_CLOSED_BY_REMOTE;
    }
    if (server->hle_handler) {
        auto file{std::dynamic_pointer_cast<Service::FS::File>(server->hle_handler)};
        // TODO: This requires RTTI, use service calls directly instead?
        if (file) {
            // Grab the session file offset in case we were given a subfile opened with
            // File::OpenSubFile
            std::size_t offset{file->GetSessionFileOffset(server)};
            std::size_t size{file->GetSessionFileSize(server)};
            return MakeResult(std::make_unique<AMFileWrapper>(file, offset, size));
        }
        LOG_ERROR(Service_AM, "Failed to cast handle to FSFile!");
        return Kernel::ERR_INVALID_HANDLE;
    }
    // Probably the best bet if someone is LLEing the fs service is to just have them LLE AM
    // while they're at it, so not implemented.
    LOG_ERROR(Service_AM, "Given file handle doesn't have an HLE handler!");
    return Kernel::ERR_NOT_IMPLEMENTED;
}

void Module::Interface::GetProgramInfoFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0408, 1, 2};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(file_res.Code());
        return;
    }
    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }
    FileSys::TitleMetadata tmd{container.GetTitleMetadata()};
    container.Print();
    // TODO: Sizes allegedly depend on the mediatype, and will double
    // on some mediatypes. Since this is more of a required install size we'll report
    // what Citra needs, but it would be good to be more accurate here.
    TitleInfo title_info{};
    title_info.pid = tmd.GetProgramID();
    title_info.size = tmd.GetContentSizeByIndex(FileSys::TMDContentIndex::Main);
    title_info.version = tmd.GetTitleVersion();
    title_info.type = tmd.GetTitleType();
    auto rb{rp.MakeBuilder(8, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<TitleInfo>(title_info);
}

void Module::Interface::GetSystemMenuDataFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0409, 0, 4};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto& output_buffer{rp.PopMappedBuffer()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(file_res.Code());
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    std::size_t output_buffer_size{std::min(output_buffer.GetSize(), sizeof(Loader::SMDH))};
    auto file{std::move(file_res.Unwrap())};
    FileSys::CIAContainer container;
    if (container.Load(*file) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    std::vector<u8> temp(output_buffer_size);
    // Read from the Meta offset + 0x400 for the 0x36C0-large SMDH
    auto read_result{file->Read(container.GetMetadataOffset() + FileSys::CIA_METADATA_SIZE,
                                temp.size(), temp.data())};
    if (read_result.Failed() || *read_result != temp.size()) {
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    output_buffer.Write(temp.data(), 0, temp.size());
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(output_buffer);
}

void Module::Interface::GetDependencyListFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x040A, 0, 2};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(file_res.Code());
        return;
    }
    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }
    std::vector<u8> buffer(FileSys::CIA_DEPENDENCY_SIZE);
    std::memcpy(buffer.data(), container.GetDependencies().data(), buffer.size());
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(buffer, 0);
}

void Module::Interface::GetTransferSizeFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x040B, 0, 2};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(file_res.Code());
        return;
    }
    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }
    auto rb{rp.MakeBuilder(3, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(container.GetMetadataOffset());
}

void Module::Interface::GetCoreVersionFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x040C, 0, 2};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(file_res.Code());
        return;
    }
    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(container.GetCoreVersion());
}

void Module::Interface::GetRequiredSizeFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x040D, 1, 2};
    auto media_type{static_cast<Service::FS::MediaType>(rp.Pop<u8>())};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(file_res.Code());
        return;
    }
    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }
    // TODO: Sizes allegedly depend on the mediatype, and will double
    // on some mediatypes. Since this is more of a required install size we'll report
    // what Citra needs, but it would be good to be more accurate here.
    auto rb{rp.MakeBuilder(3, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(container.GetTitleMetadata().GetContentSizeByIndex(FileSys::TMDContentIndex::Main));
}

void Module::Interface::DeleteProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0410, 3, 0};
    auto media_type{rp.PopEnum<FS::MediaType>()};
    u64 program_id{rp.Pop<u64>()};
    LOG_INFO(Service_AM, "Deleting program 0x{:016x}", program_id);
    auto path{GetProgramPath(media_type, program_id)};
    auto rb{rp.MakeBuilder(1, 0)};
    if (!FileUtil::Exists(path)) {
        rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        LOG_ERROR(Service_AM, "Program not found");
        return;
    }
    bool success{FileUtil::DeleteDirRecursively(path)};
    if (success)
        am->ScanForAllPrograms();
    else
        LOG_ERROR(Service_AM, "DeleteDirRecursively failed: {}", GetLastErrorMsg());
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetSystemUpdaterMutex(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x412, 1, 2};
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(am->system_updater_mutex);
}

void Module::Interface::GetMetaSizeFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0413, 0, 2};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(file_res.Code());
        return;
    }
    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(container.GetMetadataSize());
}

void Module::Interface::GetMetaDataFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0414, 1, 4};
    u32 output_size{rp.Pop<u32>()};
    auto cia{rp.PopObject<Kernel::ClientSession>()};
    auto& output_buffer{rp.PopMappedBuffer()};
    auto file_res{GetFileFromSession(cia)};
    if (!file_res.Succeeded()) {
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(file_res.Code());
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    // Don't write beyond the actual static buffer size.
    output_size = std::min(static_cast<u32>(output_buffer.GetSize()), output_size);
    auto file{std::move(file_res.Unwrap())};
    FileSys::CIAContainer container;
    if (container.Load(*file) != Loader::ResultStatus::Success) {
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    // Read from the Meta offset for the specified size
    std::vector<u8> temp(output_size);
    auto read_result{file->Read(container.GetMetadataOffset(), output_size, temp.data())};
    if (read_result.Failed() || *read_result != output_size) {
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(ResultCode(ErrCodes::InvalidCIAHeader, ErrorModule::AM,
                           ErrorSummary::InvalidArgument, ErrorLevel::Permanent));
        return;
    }

    output_buffer.Write(temp.data(), 0, output_size);
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(output_buffer);
}

void Module::Interface::GetDeviceID(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0xA, 3, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0xDEADC0DE);
    rb.Push<u32>(0xDEADC0DE);
}

void Module::Interface::DeleteUserProgramsAtomically(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0029, 2, 2};
    auto media_type{rp.PopEnum<FS::MediaType>()};
    u32 count{rp.Pop<u32>()};
    auto& buffer{rp.PopMappedBuffer()};
    u32 offset{};
    u64 program_id;
    for (u32 i{}; i < count; ++i) {
        buffer.Read(&program_id, offset, sizeof(u64));
        offset += sizeof(u64);
        u16 category{static_cast<u16>((program_id >> 32) & 0xFFFF)};
        u8 variation{static_cast<u8>(program_id & 0xFF)};
        if (category & CATEGORY_SYSTEM || category & CATEGORY_DLP || variation & VARIATION_SYSTEM) {
            LOG_ERROR(Service_AM, "Trying to uninstall system program");
            auto rb{rp.MakeBuilder(1, 0)};
            rb.Push(ResultCode(ErrCodes::TryingToUninstallSystemProgram, ErrorModule::AM,
                               ErrorSummary::InvalidArgument, ErrorLevel::Usage));
            return;
        }
        LOG_INFO(Service_AM, "Deleting program 0x{:016x}", program_id);
        auto path{GetProgramPath(media_type, program_id)};
        if (!FileUtil::Exists(path)) {
            auto rb{rp.MakeBuilder(1, 0)};
            rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::AM,
                               ErrorSummary::InvalidState, ErrorLevel::Permanent));
            LOG_ERROR(Service_AM, "Program not found");
            return;
        }
        if (!FileUtil::DeleteDirRecursively(path))
            LOG_ERROR(Service_AM, "DeleteDirRecursively failed: {}", GetLastErrorMsg());
    }
    am->ScanForAllPrograms();
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
}

Module::Module(Core::System& system) : system{system} {
    ScanForAllPrograms();
    system_updater_mutex = system.Kernel().CreateMutex(false, "AM::SystemUpdaterMutex");
}

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager{system.ServiceManager()};
    auto am{std::make_shared<Module>(system)};
    std::make_shared<AM_APP>(am)->InstallAsService(service_manager);
    std::make_shared<AM_NET>(am)->InstallAsService(service_manager);
    std::make_shared<AM_SYS>(am)->InstallAsService(service_manager);
    std::make_shared<AM_U>(am)->InstallAsService(service_manager);
}

} // namespace Service::AM
