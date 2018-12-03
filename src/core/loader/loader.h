// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/romfs_reader.h"
#include "core/hle/kernel/object.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {
struct AddressMapping;
class Process;
} // namespace Kernel

namespace Loader {

/// File types supported
enum class FileType {
    Error,
    Unknown,
    CCI,
    CXI,
    CIA,
    ELF,
    THREEDSX, // 3DSX
};

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file open file
 * @return FileType of file
 */
FileType IdentifyFile(FileUtil::IOFile& file);

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file_name path to file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType IdentifyFile(const std::string& file_name);

/**
 * Guess the type of a bootable file from its extension
 * @param extension String extension of bootable file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType GuessFromExtension(const std::string& extension);

/**
 * Convert a FileType into a string which can be displayed to the user.
 */
const char* GetFileTypeString(FileType type);

/// Return type for functions in Loader namespace
enum class ResultStatus {
    Success,
    Error,
    ErrorInvalidFormat,
    ErrorNotImplemented,
    ErrorNotLoaded,
    ErrorNotUsed,
    ErrorAlreadyLoaded,
    ErrorMemoryAllocationFailed,
    ErrorEncrypted,
};

constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return a | b << 8 | c << 16 | d << 24;
}

/// Interface for loading an program
class ProgramLoader : NonCopyable {
public:
    explicit ProgramLoader(Core::System& system, FileUtil::IOFile&& file)
        : system{system}, file{std::move(file)} {}
    virtual ~ProgramLoader() {}

    /**
     * Returns the type of this file
     * @return FileType corresponding to the loaded file
     */
    virtual FileType GetFileType() = 0;

    /**
     * Load the program and return the created Process instance
     * @param process The newly created process.
     * @return The status result of the operation.
     */
    virtual ResultStatus Load(Kernel::SharedPtr<Kernel::Process>& process) = 0;

    /**
     * Loads the system mode that this program needs.
     * This function defaults to 2 (96MB allocated to the program) if it can't read the
     * information.
     * @returns A pair with the optional system mode, and and the status.
     */
    virtual std::pair<std::optional<u32>, ResultStatus> LoadKernelSystemMode() {
        // 96MB allocated to the program.
        return std::make_pair(2, ResultStatus::Success);
    }

    /**
     * Get the code (typically .code section) of the program
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadCode(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the icon (typically icon section) of the program
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadIcon(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the banner (typically banner section) of the program
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadBanner(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the logo (typically logo section) of the program
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadLogo(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the program id of the program
     * @param out_program_id Reference to store program id into
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadProgramID(u64& out_program_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the extdata id for the program
     * @param out_extdata_id Reference to store extdata id into
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadExtdataID(u64& out_extdata_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the program
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the update RomFS of the program
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the short title of the program
     * @param short_title Reference to store the program title into
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadShortTitle(std::string& short_title) {
        return ResultStatus::ErrorNotImplemented;
    }

protected:
    FileUtil::IOFile file;
    bool is_loaded{};
    Core::System& system;
};

/**
 * Common address mappings found in most games, used for binary formats that don't have this
 * information.
 */
extern const std::initializer_list<Kernel::AddressMapping> default_address_mappings;

/**
 * Identifies a bootable file and return a suitable loader
 * @param filename String filename of bootable file
 * @return best loader for this file
 */
std::unique_ptr<ProgramLoader> GetLoader(Core::System& system, const std::string& filename);

} // namespace Loader
