// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <utility>
#include <vector>
#include "bad_word_list.app.romfs.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ivfc_archive.h"
#include "core/file_sys/ncch_container.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"
#include "country_list.app.romfs.h"
#include "shared_font.app.romfs.h"

namespace FileSys {

struct NCCHArchivePath {
    u64_le pid;
    u32_le media_type;
    u32_le unknown;
};
static_assert(sizeof(NCCHArchivePath) == 0x10, "NCCHArchivePath has wrong size!");

struct NCCHFilePath {
    enum_le<NCCHFileOpenType> open_type;
    u32_le content_index;
    enum_le<NCCHFilePathType> filepath_type;
    std::array<char, 8> exefs_filepath;
};
static_assert(sizeof(NCCHFilePath) == 0x14, "NCCHFilePath has wrong size!");

Path MakeNCCHArchivePath(u64 pid, Service::FS::MediaType media_type) {
    NCCHArchivePath path{};
    path.pid = static_cast<u64_le>(pid);
    path.media_type = static_cast<u32_le>(media_type);
    path.unknown = 0;
    std::vector<u8> archive(sizeof(path));
    std::memcpy(&archive[0], &path, sizeof(path));
    return FileSys::Path(archive);
}

Path MakeNCCHFilePath(NCCHFileOpenType open_type, u32 content_index, NCCHFilePathType filepath_type,
                      std::array<char, 8>& exefs_filepath) {
    NCCHFilePath path{};
    path.open_type = open_type;
    path.content_index = static_cast<u32_le>(content_index);
    path.filepath_type = filepath_type;
    path.exefs_filepath = exefs_filepath;
    std::vector<u8> file(sizeof(path));
    std::memcpy(&file[0], &path, sizeof(path));
    return FileSys::Path(file);
}

ResultVal<std::unique_ptr<FileBackend>> NCCHArchive::_OpenFile(const Path& path,
                                                               const Mode& mode) const {
    if (path.GetType() != LowPathType::Binary) {
        LOG_ERROR(Service_FS, "Path need to be Binary");
        return ERROR_INVALID_PATH;
    }
    std::vector<u8> binary{path.AsBinary()};
    if (binary.size() != sizeof(NCCHFilePath)) {
        LOG_ERROR(Service_FS, "Wrong path size {}", binary.size());
        return ERROR_INVALID_PATH;
    }
    NCCHFilePath openfile_path;
    std::memcpy(&openfile_path, binary.data(), sizeof(NCCHFilePath));
    std::string file_path{
        Service::AM::GetProgramContentPath(media_type, program_id, openfile_path.content_index)};
    NCCHContainer ncch_container{file_path};
    Loader::ResultStatus result;
    std::unique_ptr<FileBackend> file;
    // NCCH RomFS
    if (openfile_path.filepath_type == NCCHFilePathType::RomFS) {
        std::shared_ptr<RomFSReader> romfs_file;
        result = ncch_container.ReadRomFS(romfs_file);
        std::unique_ptr<DelayGenerator> delay_generator{std::make_unique<RomFSDelayGenerator>()};
        file = std::make_unique<IVFCFile>(std::move(romfs_file), std::move(delay_generator));
    } else if (openfile_path.filepath_type == NCCHFilePathType::Code ||
               openfile_path.filepath_type == NCCHFilePathType::ExeFS) {
        std::vector<u8> buffer;
        // Load NCCH .code or icon/banner/logo
        result = ncch_container.LoadSectionExeFS(openfile_path.exefs_filepath.data(), buffer);
        std::unique_ptr<DelayGenerator> delay_generator{std::make_unique<ExeFSDelayGenerator>()};
        file = std::make_unique<NCCHFile>(std::move(buffer), std::move(delay_generator));
    } else {
        LOG_ERROR(Service_FS, "Unknown NCCH archive type {}!",
                  static_cast<u32>(openfile_path.filepath_type));
        result = Loader::ResultStatus::Error;
    }
    if (result != Loader::ResultStatus::Success) {
        // High Program ID of the archive: The category (https://3dbrew.org/wiki/Title_list).
        constexpr u32 shared_data_archive{0x0004009B};
        constexpr u32 system_data_archive{0x000400DB};
        // Low Program IDs.
        constexpr u32 mii_data{0x00010202};
        constexpr u32 region_manifest{0x00010402};
        constexpr u32 ng_word_list{0x00010302};
        constexpr u32 shared_font{0x00014002};
        u32 high{static_cast<u32>(program_id >> 32)};
        u32 low{static_cast<u32>(program_id & 0xFFFFFFFF)};
        LOG_DEBUG(Service_FS, "Full Path: {}. Category: 0x{:X}. Path: 0x{:X}.", path.DebugStr(),
                  high, low);
        std::vector<u8> archive_data;
        if (high == shared_data_archive) {
            if (low == mii_data) {
                LOG_ERROR(Service_FS, "Failed to get a handle for shared data archive: Mii Data.");
                system.SetStatus(Core::System::ResultStatus::ErrorSystemFiles, "Mii Data");
            } else if (low == region_manifest) {
                LOG_WARNING(
                    Service_FS,
                    "Country list file missing. Loading open source replacement from memory");
                archive_data =
                    std::vector<u8>(std::begin(COUNTRY_LIST_DATA), std::end(COUNTRY_LIST_DATA));
            } else if (low == shared_font) {
                LOG_WARNING(
                    Service_FS,
                    "Shared Font file missing. Loading open source replacement from memory");
                archive_data =
                    std::vector<u8>(std::begin(SHARED_FONT_DATA), std::end(SHARED_FONT_DATA));
            }
        } else if (high == system_data_archive) {
            if (low == ng_word_list) {
                LOG_WARNING(
                    Service_FS,
                    "Bad Word List file missing. Loading open source replacement from memory");
                archive_data =
                    std::vector<u8>(std::begin(BAD_WORD_LIST_DATA), std::end(BAD_WORD_LIST_DATA));
            }
        }
        if (!archive_data.empty()) {
            u64 romfs_offset{};
            u64 romfs_size{archive_data.size()};
            std::unique_ptr<DelayGenerator> delay_generator{
                std::make_unique<RomFSDelayGenerator>()};
            file = std::make_unique<IVFCFileInMemory>(std::move(archive_data), romfs_offset,
                                                      romfs_size, std::move(delay_generator));
            return MakeResult<std::unique_ptr<FileBackend>>(std::move(file));
        }
        return ERROR_NOT_FOUND;
    }

    return MakeResult<std::unique_ptr<FileBackend>>(std::move(file));
}

ResultCode NCCHArchive::_DeleteFile(const Path& path) const {
    LOG_ERROR(Service_FS, "Attempted to delete a file from an NCCH archive ({}).", GetName());
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                      ErrorLevel::Status);
}

ResultCode NCCHArchive::_RenameFile(const Path& src_path, const Path& dest_path) const {
    LOG_ERROR(Service_FS, "Attempted to rename a file within an NCCH archive ({}).", GetName());
    // TODO: Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::_DeleteDirectory(const Path& path) const {
    LOG_ERROR(Service_FS, "Attempted to delete a directory from an NCCH archive ({}).", GetName());
    // TODO: Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::_DeleteDirectoryRecursively(const Path& path) const {
    LOG_ERROR(Service_FS, "Attempted to delete a directory from an NCCH archive ({}).", GetName());
    // TODO: Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::_CreateFile(const Path& path, u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to create a file in an NCCH archive ({}).", GetName());
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
}

ResultCode NCCHArchive::_CreateDirectory(const Path& path) const {
    LOG_ERROR(Service_FS, "Attempted to create a directory in an NCCH archive ({}).", GetName());
    // TODO: Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::_RenameDirectory(const Path& src_path, const Path& dest_path) const {
    LOG_ERROR(Service_FS, "Attempted to rename a file within an NCCH archive ({}).", GetName());
    // TODO: Use correct error code
    return ResultCode(-1);
}

ResultVal<std::unique_ptr<DirectoryBackend>> NCCHArchive::_OpenDirectory(const Path& path) const {
    LOG_ERROR(Service_FS, "Attempted to open a directory within an NCCH archive ({}).",
              GetName().c_str());
    // TODO: Use correct error code
    return ResultCode(-1);
}

u64 NCCHArchive::GetFreeBytes() const {
    LOG_WARNING(Service_FS, "Attempted to get the free space in an NCCH archive");
    return 0;
}

NCCHFile::NCCHFile(std::vector<u8> buffer, std::unique_ptr<DelayGenerator> delay_generator_)
    : file_buffer{std::move(buffer)} {
    delay_generator = std::move(delay_generator_);
}

ResultVal<std::size_t> NCCHFile::Read(const u64 offset, const std::size_t length,
                                      u8* buffer) const {
    LOG_TRACE(Service_FS, "offset={}, length={}", offset, length);
    std::size_t length_left{static_cast<std::size_t>(data_size - offset)};
    std::size_t read_length{static_cast<std::size_t>(std::min(length, length_left))};
    std::size_t available_size{static_cast<std::size_t>(file_buffer.size() - offset)};
    std::size_t copy_size{std::min(length, available_size)};
    std::memcpy(buffer, file_buffer.data() + offset, copy_size);
    return MakeResult<std::size_t>(copy_size);
}

ResultVal<std::size_t> NCCHFile::Write(const u64 offset, const std::size_t length, const bool flush,
                                       const u8* buffer) {
    LOG_ERROR(Service_FS, "Attempted to write to NCCH file");
    // TODO: Find error code
    return MakeResult<std::size_t>(0);
}

u64 NCCHFile::GetSize() const {
    return file_buffer.size();
}

bool NCCHFile::SetSize(const u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to set the size of an NCCH file");
    return false;
}

ArchiveFactory_NCCH::ArchiveFactory_NCCH(Core::System& system) : system{system} {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_NCCH::Open(const Path& path) {
    if (path.GetType() != LowPathType::Binary) {
        LOG_ERROR(Service_FS, "Path need to be Binary");
        return ERROR_INVALID_PATH;
    }
    std::vector<u8> binary{path.AsBinary()};
    if (binary.size() != sizeof(NCCHArchivePath)) {
        LOG_ERROR(Service_FS, "Wrong path size {}", binary.size());
        return ERROR_INVALID_PATH;
    }
    NCCHArchivePath open_path;
    std::memcpy(&open_path, binary.data(), sizeof(NCCHArchivePath));
    auto archive{std::make_unique<NCCHArchive>(
        system, open_path.pid, static_cast<Service::FS::MediaType>(open_path.media_type & 0xFF))};
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_NCCH::Format(const Path& path,
                                       const FileSys::ArchiveFormatInfo& format_info) {
    LOG_ERROR(Service_FS, "Attempted to format a NCCH archive.");
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_NCCH::GetFormatInfo(const Path& path) const {
    // TODO: Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive {}", GetName());
    return ResultCode(-1);
}

} // namespace FileSys
