// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <codecvt>
#include <cstring>
#include <locale>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/ncch.h"
#include "core/loader/smdh.h"
#include "core/memory.h"
#include "network/room_member.h"

namespace Loader {

constexpr u64 UPDATE_MASK{0x0000000e00000000};

FileType ProgramLoader_NCCH::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0x100, SEEK_SET);
    if (file.ReadArray<u32>(&magic, 1) != 1)
        return FileType::Error;
    if (MakeMagic('N', 'C', 'S', 'D') == magic)
        return FileType::CCI;
    if (MakeMagic('N', 'C', 'C', 'H') == magic)
        return FileType::CXI;
    return FileType::Error;
}

std::pair<std::optional<u32>, ResultStatus> ProgramLoader_NCCH::LoadKernelSystemMode() {
    if (!is_loaded) {
        auto res{base_ncch.Load()};
        if (res != ResultStatus::Success)
            return std::make_pair(std::nullopt, res);
    }
    // Set the system mode as the one from the exheader.
    return std::make_pair(overlay_ncch->exheader_header.arm11_system_local_caps.system_mode.Value(),
                          ResultStatus::Success);
}

ResultStatus ProgramLoader_NCCH::LoadExec(Kernel::SharedPtr<Kernel::Process>& process) {
    using Kernel::CodeSet;
    using Kernel::SharedPtr;
    if (!is_loaded)
        return ResultStatus::ErrorNotLoaded;
    std::vector<u8> code;
    u64_le program_id;
    if (ResultStatus::Success == ReadCode(code) &&
        ResultStatus::Success == ReadProgramID(program_id)) {
        auto process_name{Common::StringFromFixedZeroTerminatedBuffer(
            (const char*)overlay_ncch->exheader_header.codeset_info.name, 8)};
        auto codeset{system.Kernel().CreateCodeSet(process_name, program_id)};
        codeset->CodeSegment().offset = 0;
        codeset->CodeSegment().addr = overlay_ncch->exheader_header.codeset_info.text.address;
        codeset->CodeSegment().size =
            overlay_ncch->exheader_header.codeset_info.text.num_max_pages * Memory::PAGE_SIZE;
        codeset->RODataSegment().offset =
            codeset->CodeSegment().offset + codeset->CodeSegment().size;
        codeset->RODataSegment().addr = overlay_ncch->exheader_header.codeset_info.ro.address;
        codeset->RODataSegment().size =
            overlay_ncch->exheader_header.codeset_info.ro.num_max_pages * Memory::PAGE_SIZE;
        // TODO: Not sure if the bss size is added to the page-aligned .data size or just
        //               to the regular size. Playing it safe for now.
        u32 bss_page_size{(overlay_ncch->exheader_header.codeset_info.bss_size + 0xFFF) & ~0xFFF};
        code.resize(code.size() + bss_page_size, 0);
        codeset->DataSegment().offset =
            codeset->RODataSegment().offset + codeset->RODataSegment().size;
        codeset->DataSegment().addr = overlay_ncch->exheader_header.codeset_info.data.address;
        codeset->DataSegment().size =
            overlay_ncch->exheader_header.codeset_info.data.num_max_pages * Memory::PAGE_SIZE +
            bss_page_size;
        codeset->entrypoint = codeset->CodeSegment().addr;
        codeset->memory = std::make_shared<std::vector<u8>>(std::move(code));
        auto& kernel{system.Kernel()};
        process = kernel.CreateProcess(std::move(codeset));
        // Attach a resource limit to the process based on the resource limit category
        process->resource_limit =
            kernel.ResourceLimit().GetForCategory(static_cast<Kernel::ResourceLimitCategory>(
                overlay_ncch->exheader_header.arm11_system_local_caps.resource_limit_category));
        // Set the default CPU core for this process
        process->ideal_processor =
            overlay_ncch->exheader_header.arm11_system_local_caps.ideal_processor;
        // Copy data while converting endianness
        std::array<u32, ARRAY_SIZE(overlay_ncch->exheader_header.arm11_kernel_caps.descriptors)>
            kernel_caps;
        std::copy_n(overlay_ncch->exheader_header.arm11_kernel_caps.descriptors, kernel_caps.size(),
                    begin(kernel_caps));
        process->ParseKernelCaps(kernel_caps.data(), kernel_caps.size());
        s32 priority{overlay_ncch->exheader_header.arm11_system_local_caps.priority};
        u32 stack_size{overlay_ncch->exheader_header.codeset_info.stack_size};
        process->Run(priority, stack_size);
        return ResultStatus::Success;
    }
    return ResultStatus::Error;
}

void ProgramLoader_NCCH::ParseRegionLockoutInfo() {
    std::vector<u8> smdh_buffer;
    if (ReadIcon(smdh_buffer) == ResultStatus::Success && smdh_buffer.size() >= sizeof(SMDH)) {
        SMDH smdh;
        std::memcpy(&smdh, smdh_buffer.data(), sizeof(SMDH));
        u32 region_lockout{smdh.region_lockout};
        constexpr u32 REGION_COUNT{7};
        std::vector<u32> regions;
        for (u32 region{}; region < REGION_COUNT; ++region) {
            if (region_lockout & 1)
                regions.push_back(region);
            region_lockout >>= 1;
        }
        system.ServiceManager()
            .GetService<Service::CFG::Module::Interface>("cfg:u")
            ->GetModule()
            ->SetPreferredRegionCodes(regions);
    }
}

ResultStatus ProgramLoader_NCCH::Load(Kernel::SharedPtr<Kernel::Process>& process) {
    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;
    auto result{base_ncch.Load()};
    if (result != ResultStatus::Success)
        return result;
    u64_le ncch_program_id;
    ReadProgramID(ncch_program_id);
    auto program_id{fmt::format("{:016X}", ncch_program_id)};
    LOG_INFO(Loader, "Program ID: {}", program_id);
    update_ncch.OpenFile(Service::AM::GetProgramContentPath(Service::FS::MediaType::SDMC,
                                                            ncch_program_id | UPDATE_MASK));
    result = update_ncch.Load();
    if (result == ResultStatus::Success)
        overlay_ncch = &update_ncch;
    std::string program;
    ReadShortTitle(program);
    system.RoomMember().SendProgram(program);
    is_loaded = true;           // Set state to loaded
    result = LoadExec(process); // Load the executable into memory for booting
    if (ResultStatus::Success != result)
        return result;
    system.ArchiveManager().RegisterSelfNCCH(*this);
    ParseRegionLockoutInfo();
    return ResultStatus::Success;
}

ResultStatus ProgramLoader_NCCH::ReadCode(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS(".code", buffer);
}

ResultStatus ProgramLoader_NCCH::ReadIcon(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("icon", buffer);
}

ResultStatus ProgramLoader_NCCH::ReadBanner(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("banner", buffer);
}

ResultStatus ProgramLoader_NCCH::ReadLogo(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("logo", buffer);
}

ResultStatus ProgramLoader_NCCH::ReadProgramID(u64& out_program_id) {
    auto result{base_ncch.ReadProgramID(out_program_id)};
    if (result != ResultStatus::Success)
        return result;
    return ResultStatus::Success;
}

ResultStatus ProgramLoader_NCCH::ReadExtdataID(u64& out_extdata_id) {
    auto result{base_ncch.ReadExtdataID(out_extdata_id)};
    if (result != ResultStatus::Success)
        return result;
    return ResultStatus::Success;
}

ResultStatus ProgramLoader_NCCH::ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    return base_ncch.ReadRomFS(romfs_file);
}

ResultStatus ProgramLoader_NCCH::ReadUpdateRomFS(
    std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    auto result{update_ncch.ReadRomFS(romfs_file)};
    if (result != ResultStatus::Success)
        return base_ncch.ReadRomFS(romfs_file);
    return ResultStatus::Success;
}

ResultStatus ProgramLoader_NCCH::ReadShortTitle(std::string& short_title) {
    std::vector<u8> data;
    Loader::SMDH smdh;
    ReadIcon(data);
    if (!Loader::IsValidSMDH(data))
        return ResultStatus::ErrorInvalidFormat;
    std::memcpy(&smdh, data.data(), sizeof(Loader::SMDH));
    const auto& short_title_array{smdh.GetShortTitle(SMDH::TitleLanguage::English)};
    auto end{std::find(short_title_array.begin(), short_title_array.end(), u'\0')};
    short_title = Common::UTF16ToUTF8(std::u16string{short_title_array.begin(), end});
    return ResultStatus::Success;
}

} // namespace Loader
