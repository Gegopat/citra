// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/seed_db.h"

namespace FileSys {

bool SeedDB::Load() {
    seeds.clear();
    const auto path{
        fmt::format("{}seeddb.bin", FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir))};
    if (!FileUtil::Exists(path)) {
        if (!FileUtil::CreateFullPath(path)) {
            LOG_ERROR(Service_FS, "Failed to create seed database");
            return false;
        }
        if (!Save()) {
            LOG_ERROR(Service_FS, "Failed to save seed database");
            return false;
        }
        return true;
    }
    FileUtil::IOFile file{path, "rb"};
    if (!file.IsOpen()) {
        LOG_ERROR(Service_FS, "Failed to open seed database");
        return false;
    }
    u32 count;
    if (!file.ReadBytes(&count, sizeof(count))) {
        LOG_ERROR(Service_FS, "Failed to read seed database count fully");
        return false;
    }
    seeds.resize(count);
    if (!file.Seek(SEEDDB_PADDING_BYTES, SEEK_CUR)) {
        LOG_ERROR(Service_FS, "Failed to skip seed database padding");
        return false;
    }
    if (file.ReadArray(&seeds[0], count) != count) {
        LOG_ERROR(Log, "Invalid seed database file");
        return false;
    }
    return true;
}

bool SeedDB::Save() {
    const auto path{
        fmt::format("{}seeddb.bin", FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir))};
    if (!FileUtil::CreateFullPath(path)) {
        LOG_ERROR(Service_FS, "Failed to create seed database");
        return false;
    }
    FileUtil::IOFile file{path, "wb"};
    if (!file.IsOpen()) {
        LOG_ERROR(Service_FS, "Failed to open seed database");
        return false;
    }
    u32 count{static_cast<u32>(seeds.size())};
    if (file.WriteBytes(&count, sizeof(count)) != sizeof(count)) {
        LOG_ERROR(Service_FS, "Failed to write seed database count fully");
        return false;
    }
    std::array<u8, SEEDDB_PADDING_BYTES> padding{};
    if (file.WriteBytes(padding.data(), padding.size()) != padding.size()) {
        LOG_ERROR(Service_FS, "Failed to write seed database padding fully");
        return false;
    }
    if (file.WriteArray(&seeds[0], count) != count) {
        LOG_ERROR(Service_FS, "Error when writing seeds");
        return false;
    }
    return true;
}

void SeedDB::Add(const Seed& seed) {
    seeds.push_back(seed);
}

std::size_t SeedDB::GetCount() const {
    return seeds.size();
}

auto SeedDB::FindSeedByProgramID(u64 program_id) const {
    return std::find_if(seeds.begin(), seeds.end(),
                        [program_id](const auto& seed) { return seed.program_id == program_id; });
}

bool AddSeed(const Seed& seed) {
    // TODO: does this skip/replace if the SeedDB contains a seed for seed.program_id?
    SeedDB db;
    if (!db.Load()) {
        LOG_ERROR(Service_FS, "Failed to load seed database");
        return false;
    }
    db.Add(seed);
    if (!db.Save()) {
        LOG_ERROR(Service_FS, "Failed to save seed database");
        return false;
    }
    return true;
}

std::optional<Seed::Data> GetSeed(u64 program_id) {
    SeedDB db;
    if (!db.Load())
        return {};
    const auto found_seed_iter{db.FindSeedByProgramID(program_id)};
    if (found_seed_iter != db.seeds.end())
        return found_seed_iter->data;
    return {};
}

std::size_t GetSeedCount() {
    SeedDB db;
    if (!db.Load())
        return 0;
    return db.GetCount();
}

} // namespace FileSys
