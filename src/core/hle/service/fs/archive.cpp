// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/archive_other_savedata.h"
#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/archive_sdmcwriteonly.h"
#include "core/file_sys/archive_selfncch.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"

namespace Service::FS {

ArchiveBackend* ArchiveManager::GetArchive(ArchiveHandle handle) {
    auto itr{handle_map.find(handle)};
    return (itr == handle_map.end()) ? nullptr : itr->second.get();
}

ResultVal<ArchiveHandle> ArchiveManager::OpenArchive(ArchiveIDCode id_code,
                                                     FileSys::Path& archive_path) {
    LOG_TRACE(Service_FS, "Opening archive with id code 0x{:08X}", static_cast<u32>(id_code));
    auto itr{id_code_map.find(id_code)};
    if (itr == id_code_map.end())
        return FileSys::ERROR_NOT_FOUND;
    CASCADE_RESULT(std::unique_ptr<ArchiveBackend> res, itr->second->Open(archive_path));
    // This should never even happen in the first place with 64-bit handles,
    while (handle_map.count(next_handle) != 0)
        ++next_handle;
    handle_map.emplace(next_handle, std::move(res));
    return MakeResult<ArchiveHandle>(next_handle++);
}

ResultCode ArchiveManager::CloseArchive(ArchiveHandle handle) {
    if (handle_map.erase(handle) == 0)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    else
        return RESULT_SUCCESS;
}

// TODO: This might be what the fs:REG service is for. See the Register/Unregister calls in
// http://3dbrew.org/wiki/Filesystem_services#ProgramRegistry_service_.22fs:REG.22
ResultCode ArchiveManager::RegisterArchiveType(std::unique_ptr<FileSys::ArchiveFactory>&& factory,
                                               ArchiveIDCode id_code) {
    auto result{id_code_map.emplace(id_code, std::move(factory))};
    bool inserted{result.second};
    ASSERT_MSG(inserted, "Tried to register more than one archive with same id code");
    auto& archive{result.first->second};
    LOG_DEBUG(Service_FS, "Registered archive {} with id code 0x{:08X}", archive->GetName(),
              static_cast<u32>(id_code));
    return RESULT_SUCCESS;
}

std::tuple<ResultVal<std::shared_ptr<File>>, std::chrono::nanoseconds>
ArchiveManager::OpenFileFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path,
                                    const FileSys::Mode mode) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return std::make_tuple(FileSys::ERR_INVALID_ARCHIVE_HANDLE, std::chrono::nanoseconds{0});
    std::chrono::nanoseconds open_timeout_ns{archive->GetOpenDelayNs()};
    auto backend{archive->OpenFile(path, mode)};
    if (backend.Failed())
        return std::make_tuple(backend.Code(), open_timeout_ns);
    auto file{std::shared_ptr<File>(new File(system, std::move(backend).Unwrap(), path))};
    return std::make_tuple(MakeResult<std::shared_ptr<File>>(std::move(file)), open_timeout_ns);
}

ResultCode ArchiveManager::DeleteFileFromArchive(ArchiveHandle archive_handle,
                                                 const FileSys::Path& path) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return archive->DeleteFile(path);
}

ResultCode ArchiveManager::RenameFileBetweenArchives(ArchiveHandle src_archive_handle,
                                                     const FileSys::Path& src_path,
                                                     ArchiveHandle dest_archive_handle,
                                                     const FileSys::Path& dest_path) {
    ArchiveBackend* src_archive{GetArchive(src_archive_handle)};
    ArchiveBackend* dest_archive{GetArchive(dest_archive_handle)};
    if (!src_archive || !dest_archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    if (src_archive == dest_archive)
        return src_archive->RenameFile(src_path, dest_path);
    else
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
}

ResultCode ArchiveManager::DeleteDirectoryFromArchive(ArchiveHandle archive_handle,
                                                      const FileSys::Path& path) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return archive->DeleteDirectory(path);
}

ResultCode ArchiveManager::DeleteDirectoryRecursivelyFromArchive(ArchiveHandle archive_handle,
                                                                 const FileSys::Path& path) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return archive->DeleteDirectoryRecursively(path);
}

ResultCode ArchiveManager::CreateFileInArchive(ArchiveHandle archive_handle,
                                               const FileSys::Path& path, u64 file_size) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return archive->CreateFile(path, file_size);
}

ResultCode ArchiveManager::CreateDirectoryFromArchive(ArchiveHandle archive_handle,
                                                      const FileSys::Path& path) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return archive->CreateDirectory(path);
}

ResultCode ArchiveManager::RenameDirectoryBetweenArchives(ArchiveHandle src_archive_handle,
                                                          const FileSys::Path& src_path,
                                                          ArchiveHandle dest_archive_handle,
                                                          const FileSys::Path& dest_path) {
    ArchiveBackend* src_archive{GetArchive(src_archive_handle)};
    ArchiveBackend* dest_archive{GetArchive(dest_archive_handle)};
    if (!src_archive || !dest_archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    if (src_archive == dest_archive)
        return src_archive->RenameDirectory(src_path, dest_path);
    else
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
}

ResultVal<std::shared_ptr<Directory>> ArchiveManager::OpenDirectoryFromArchive(
    ArchiveHandle archive_handle, const FileSys::Path& path) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    auto backend{archive->OpenDirectory(path)};
    if (backend.Failed())
        return backend.Code();
    auto directory{std::shared_ptr<Directory>(new Directory(std::move(backend).Unwrap(), path))};
    return MakeResult<std::shared_ptr<Directory>>(std::move(directory));
}

ResultVal<u64> ArchiveManager::GetFreeBytesInArchive(ArchiveHandle archive_handle) {
    ArchiveBackend* archive{GetArchive(archive_handle)};
    if (!archive)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return MakeResult<u64>(archive->GetFreeBytes());
}

ResultCode ArchiveManager::FormatArchive(ArchiveIDCode id_code,
                                         const FileSys::ArchiveFormatInfo& format_info,
                                         const FileSys::Path& path) {
    auto archive{id_code_map.find(id_code)};
    if (archive == id_code_map.end())
        return UnimplementedFunction(ErrorModule::FS); // TODO: Find the right error
    return archive->second->Format(path, format_info);
}

ResultVal<FileSys::ArchiveFormatInfo> ArchiveManager::GetArchiveFormatInfo(
    ArchiveIDCode id_code, FileSys::Path& archive_path) {
    auto archive{id_code_map.find(id_code)};
    if (archive == id_code_map.end())
        return UnimplementedFunction(ErrorModule::FS); // TODO: Find the right error
    return archive->second->GetFormatInfo(archive_path);
}

ResultCode ArchiveManager::CreateExtSaveData(MediaType media_type, u32 high, u32 low,
                                             const std::vector<u8>& smdh_icon,
                                             const FileSys::ArchiveFormatInfo& format_info) {
    // Construct the binary path to the archive first
    FileSys::Path path{
        FileSys::ConstructExtDataBinaryPath(static_cast<u32>(media_type), high, low)};
    auto archive{id_code_map.find(media_type == MediaType::NAND ? ArchiveIDCode::SharedExtSaveData
                                                                : ArchiveIDCode::ExtSaveData)};
    if (archive == id_code_map.end())
        return UnimplementedFunction(ErrorModule::FS); // TODO: Find the right error
    auto ext_savedata{static_cast<FileSys::ArchiveFactory_ExtSaveData*>(archive->second.get())};
    auto result{ext_savedata->Format(path, format_info)};
    if (result.IsError())
        return result;
    ext_savedata->WriteIcon(path, smdh_icon.data(), smdh_icon.size());
    return RESULT_SUCCESS;
}

ResultCode ArchiveManager::DeleteExtSaveData(MediaType media_type, u32 high, u32 low) {
    // Construct the binary path to the archive first
    FileSys::Path path{
        FileSys::ConstructExtDataBinaryPath(static_cast<u32>(media_type), high, low)};
    std::string media_type_directory;
    if (media_type == MediaType::NAND)
        media_type_directory =
            FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir);
    else if (media_type == MediaType::SDMC)
        media_type_directory =
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir);
    else {
        LOG_ERROR(Service_FS, "Unsupported media type {}", static_cast<u32>(media_type));
        return ResultCode(-1); // TODO: Find the right error code
    }
    // Delete all directories (/user, /boss) and the icon file.
    std::string base_path{
        FileSys::GetExtDataContainerPath(media_type_directory, media_type == MediaType::NAND)};
    std::string extsavedata_path{FileSys::GetExtSaveDataPath(base_path, path)};
    if (FileUtil::Exists(extsavedata_path) && !FileUtil::DeleteDirRecursively(extsavedata_path))
        return ResultCode(-1); // TODO: Find the right error code
    return RESULT_SUCCESS;
}

ResultCode ArchiveManager::DeleteSystemSaveData(u32 high, u32 low) {
    // Construct the binary path to the archive first
    FileSys::Path path{FileSys::ConstructSystemSaveDataBinaryPath(high, low)};
    auto nand_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir)};
    auto base_path{FileSys::GetSystemSaveDataContainerPath(nand_directory)};
    auto systemsavedata_path{FileSys::GetSystemSaveDataPath(base_path, path)};
    if (!FileUtil::DeleteDirRecursively(systemsavedata_path))
        return ResultCode(-1); // TODO: Find the right error code
    return RESULT_SUCCESS;
}

ResultCode ArchiveManager::CreateSystemSaveData(u32 high, u32 low) {
    // Construct the binary path to the archive first
    FileSys::Path path{FileSys::ConstructSystemSaveDataBinaryPath(high, low)};
    auto nand_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir)};
    auto base_path{FileSys::GetSystemSaveDataContainerPath(nand_directory)};
    auto systemsavedata_path{FileSys::GetSystemSaveDataPath(base_path, path)};
    if (!FileUtil::CreateFullPath(systemsavedata_path))
        return ResultCode(-1); // TODO: Find the right error code
    return RESULT_SUCCESS;
}

void ArchiveManager::RegisterArchiveTypes() {
    // TODO: Add the other archive types (see here for the known types:
    // http://3dbrew.org/wiki/FS:OpenArchive#Archive_idcodes).
    auto sdmc_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir)};
    auto nand_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir)};
    auto sdmc_factory{std::make_unique<FileSys::ArchiveFactory_SDMC>(sdmc_directory)};
    if (sdmc_factory->Initialize())
        RegisterArchiveType(std::move(sdmc_factory), ArchiveIDCode::SDMC);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMC archive with path {}", sdmc_directory);
    auto sdmcwo_factory{std::make_unique<FileSys::ArchiveFactory_SDMCWriteOnly>(sdmc_directory)};
    if (sdmcwo_factory->Initialize())
        RegisterArchiveType(std::move(sdmcwo_factory), ArchiveIDCode::SDMCWriteOnly);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMCWriteOnly archive with path {}",
                  sdmc_directory);
    // Create the SaveData archive
    auto sd_savedata_source{std::make_shared<FileSys::ArchiveSource_SDSaveData>(sdmc_directory)};
    auto savedata_factory{
        std::make_unique<FileSys::ArchiveFactory_SaveData>(system, sd_savedata_source)};
    RegisterArchiveType(std::move(savedata_factory), ArchiveIDCode::SaveData);
    // Create the OtherSaveDataPermitted archive
    auto other_savedata_permitted_factory{
        std::make_unique<FileSys::ArchiveFactory_OtherSaveDataPermitted>(sd_savedata_source)};
    RegisterArchiveType(std::move(other_savedata_permitted_factory),
                        ArchiveIDCode::OtherSaveDataPermitted);
    // Create the OtherSaveDataGeneral archive
    auto other_savedata_general_factory{
        std::make_unique<FileSys::ArchiveFactory_OtherSaveDataGeneral>(sd_savedata_source)};
    RegisterArchiveType(std::move(other_savedata_general_factory),
                        ArchiveIDCode::OtherSaveDataGeneral);
    // Create the ExtSaveData archive
    auto extsavedata_factory{
        std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(sdmc_directory, false)};
    RegisterArchiveType(std::move(extsavedata_factory), ArchiveIDCode::ExtSaveData);
    // Create the shared ExtSaveData archive
    auto sharedextsavedata_factory{
        std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(nand_directory, true)};
    RegisterArchiveType(std::move(sharedextsavedata_factory), ArchiveIDCode::SharedExtSaveData);
    // Create the NCCH archive, basically a small variation of the RomFS archive
    auto savedatacheck_factory{std::make_unique<FileSys::ArchiveFactory_NCCH>(system)};
    RegisterArchiveType(std::move(savedatacheck_factory), ArchiveIDCode::NCCH);
    // Create the SystemSaveData archive
    auto systemsavedata_factory{
        std::make_unique<FileSys::ArchiveFactory_SystemSaveData>(nand_directory)};
    RegisterArchiveType(std::move(systemsavedata_factory), ArchiveIDCode::SystemSaveData);
    // Create the SelfNCCH archive
    auto selfncch_factory{std::make_unique<FileSys::ArchiveFactory_SelfNCCH>(system)};
    RegisterArchiveType(std::move(selfncch_factory), ArchiveIDCode::SelfNCCH);
}

void ArchiveManager::RegisterSelfNCCH(Loader::ProgramLoader& program_loader) {
    auto itr{id_code_map.find(ArchiveIDCode::SelfNCCH)};
    if (itr == id_code_map.end()) {
        LOG_ERROR(Service_FS,
                  "Couldn't register a new NCCH because the SelfNCCH archive hasn't been created");
        return;
    }
    auto factory{static_cast<FileSys::ArchiveFactory_SelfNCCH*>(itr->second.get())};
    factory->Register(program_loader);
}

ArchiveManager::ArchiveManager(Core::System& system) : system(system) {
    RegisterArchiveTypes();
}

} // namespace Service::FS
