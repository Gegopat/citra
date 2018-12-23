// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"

namespace Core {
class System;
} // namespace Core

namespace Service::FS {
enum class MediaType : u32;
} // namespace Service::FS

namespace FileSys {

enum class NCCHFilePathType : u32 {
    RomFS = 0,
    Code = 1,
    ExeFS = 2,
};

enum class NCCHFileOpenType : u32 {
    NCCHData = 0,
    SaveData = 1,
};

/// Archive backend for NCCH Archives (RomFS, ExeFS)
class NCCHArchive : public ArchiveBackend {
public:
    explicit NCCHArchive(Core::System& system, u64 program_id, Service::FS::MediaType media_type)
        : system{system}, program_id{program_id}, media_type{media_type} {}

    std::string GetName() const override {
        return "NCCHArchive";
    }

    ResultVal<std::unique_ptr<FileBackend>> _OpenFile(const Path& path,
                                                      const Mode& mode) const override;
    ResultCode _DeleteFile(const Path& path) const override;
    ResultCode _RenameFile(const Path& src_path, const Path& dest_path) const override;
    ResultCode _DeleteDirectory(const Path& path) const override;
    ResultCode _DeleteDirectoryRecursively(const Path& path) const override;
    ResultCode _CreateFile(const Path& path, u64 size) const override;
    ResultCode _CreateDirectory(const Path& path) const override;
    ResultCode _RenameDirectory(const Path& src_path, const Path& dest_path) const override;
    ResultVal<std::unique_ptr<DirectoryBackend>> _OpenDirectory(const Path& path) const override;

    u64 GetFreeBytes() const override;

protected:
    u64 program_id;
    Service::FS::MediaType media_type;
    Core::System& system;
};

// File backend for NCCH files
class NCCHFile : public FileBackend {
public:
    explicit NCCHFile(std::vector<u8> buffer, std::unique_ptr<DelayGenerator> delay_generator_);

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                 const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() const override {
        return false;
    }
    void Flush() const override {}

private:
    std::vector<u8> file_buffer;
    u64 data_offset;
    u64 data_size;
};

/// File system interface to the NCCH archive
class ArchiveFactory_NCCH final : public ArchiveFactory {
public:
    explicit ArchiveFactory_NCCH(Core::System& system);

    std::string GetName() const override {
        return "NCCH";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;

private:
    Core::System& system;
};

Path MakeNCCHFilePath(NCCHFileOpenType open_type, u32 content_index, NCCHFilePathType filepath_type,
                      std::array<char, 8>& exefs_filepath);

} // namespace FileSys
