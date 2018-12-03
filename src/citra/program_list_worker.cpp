// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QFileInfo>
#include "citra/program_list.h"
#include "citra/program_list_p.h"
#include "citra/program_list_worker.h"
#include "citra/ui_settings.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"

namespace {

static bool HasSupportedFileExtension(const std::string& file_name) {
    QFileInfo file{QString::fromStdString(file_name)};
    return ProgramList::supported_file_extensions.contains(file.suffix(), Qt::CaseInsensitive);
}

} // Anonymous namespace

void ProgramListWorker::AddFstEntriesToProgramList(const std::string& dir_path,
                                                   unsigned int recursion,
                                                   ProgramListDir* parent_dir) {
    const auto callback{[this, recursion, parent_dir](u64* num_entries_out,
                                                      const std::string& directory,
                                                      const std::string& virtual_name) -> bool {
        auto physical_name{fmt::format("{}/{}", directory, virtual_name)};
        if (stop_processing)
            return false; // Breaks the callback loop.
        bool is_dir{FileUtil::IsDirectory(physical_name)};
        if (!is_dir && HasSupportedFileExtension(physical_name)) {
            auto loader{Loader::GetLoader(system, physical_name)};
            if (!loader)
                return true;
            u64 program_id;
            loader->ReadProgramID(program_id);
            u64 extdata_id;
            loader->ReadExtdataID(extdata_id);
            auto smdh{[this, program_id, &loader]() -> std::vector<u8> {
                std::vector<u8> original_smdh;
                loader->ReadIcon(original_smdh);
                if (program_id < 0x0004000000000000 || program_id > 0x00040000FFFFFFFF)
                    return original_smdh;
                auto update_path{Service::AM::GetProgramContentPath(
                    Service::FS::MediaType::SDMC, program_id + 0x0000000E00000000)};
                if (!FileUtil::Exists(update_path))
                    return original_smdh;
                auto update_loader{Loader::GetLoader(system, update_path)};
                if (!update_loader)
                    return original_smdh;
                std::vector<u8> update_smdh;
                update_loader->ReadIcon(update_smdh);
                return update_smdh;
            }()};
            if (!Loader::IsValidSMDH(smdh) && UISettings::values.program_list_hide_no_icon)
                // Skip this invalid entry
                return true;
            emit EntryReady(
                {
                    new ProgramListItemPath(QString::fromStdString(physical_name), smdh, program_id,
                                            extdata_id),
                    new ProgramListItemIssues(program_id),
                    new ProgramListItemRegion(smdh),
                    new ProgramListItem(
                        QString::fromStdString(Loader::GetFileTypeString(loader->GetFileType()))),
                    new ProgramListItemSize(FileUtil::GetSize(physical_name)),
                },
                parent_dir);
        } else if (is_dir && recursion > 0) {
            watch_list.append(QString::fromStdString(physical_name));
            AddFstEntriesToProgramList(physical_name, recursion - 1, parent_dir);
        }
        return true;
    }};
    FileUtil::ForeachDirectoryEntry(nullptr, dir_path, callback);
}

void ProgramListWorker::run() {
    stop_processing = false;
    for (auto& program_dir : program_dirs) {
        if (program_dir.path == "INSTALLED") {
            // Add normal programs
            {
                auto path{QString::fromStdString(
                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir) +
                    "Nintendo "
                    "3DS/00000000000000000000000000000000/"
                    "00000000000000000000000000000000/title/00040000")};
                watch_list.append(path);
                auto program_list_dir{
                    new ProgramListDir(program_dir, ProgramListItemType::InstalledDir)};
                emit DirEntryReady({program_list_dir});
                AddFstEntriesToProgramList(path.toStdString(), 2, program_list_dir);
            }
            // Add demos
            {
                auto path{QString::fromStdString(
                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir) +
                    "Nintendo "
                    "3DS/00000000000000000000000000000000/"
                    "00000000000000000000000000000000/title/00040002")};
                watch_list.append(path);
                auto program_list_dir{
                    new ProgramListDir(program_dir, ProgramListItemType::InstalledDir)};
                emit DirEntryReady({program_list_dir});
                AddFstEntriesToProgramList(path.toStdString(), 2, program_list_dir);
            }
        } else if (program_dir.path == "SYSTEM") {
            auto path{QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir,
                                                                   Settings::values.nand_dir)) +
                      "00000000000000000000000000000000/title/00040010"};
            watch_list.append(path);
            auto program_list_dir{new ProgramListDir(program_dir, ProgramListItemType::SystemDir)};
            emit DirEntryReady({program_list_dir});
            AddFstEntriesToProgramList(path.toStdString(), 2, program_list_dir);
        } else {
            watch_list.append(program_dir.path);
            auto program_list_dir{new ProgramListDir(program_dir)};
            emit DirEntryReady({program_list_dir});
            AddFstEntriesToProgramList(program_dir.path.toStdString(),
                                       program_dir.deep_scan ? 256 : 0, program_list_dir);
        }
    }
    emit Finished(watch_list);
}

void ProgramListWorker::Cancel() {
    disconnect();
    stop_processing = true;
}
