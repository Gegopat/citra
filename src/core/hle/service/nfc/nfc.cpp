// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/process.h"
#include "core/hle/lock.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

namespace Service::NFC {

#define AMIIBO_YEAR_FROM_DATE(dt) ((dt >> 1) + 2000)
#define AMIIBO_MONTH_FROM_DATE(dt) ((*((&dt) + 1) >> 5) & 0xF)
#define AMIIBO_DAY_FROM_DATE(dt) (*((&dt) + 1) & 0x1F)

struct TagInfo {
    u16_le id_offset_size; /// u16 size/offset of the below ID data. Normally this is 0x7. When
                           /// this is <=10, this field is the size of the below ID data. When this
                           /// is >10, this is the offset of the 10-byte ID data, relative to
                           /// structstart+4+<offsetfield-10>. It's unknown in what cases this
                           /// 10-byte ID data is used.
    u8 unk_x2;             // Unknown u8, normally 0x0.
    u8 unk_x3;             // Unknown u8, normally 0x2.
    std::array<u8, 0x28> id; // ID data. When the above size field is 0x7, this is the 7-byte NFC
                             // tag UID, followed by all-zeros.
};

struct AmiiboConfig {
    u16_le lastwritedate_year;
    u8 lastwritedate_month;
    u8 lastwritedate_day;
    u16_le write_counter;
    std::array<u8, 3> character_id; /// The first element is the collection ID, the second the
                                    /// character in this collection, the third the variant
    u8 series;                      /// ID of the series
    u16_le amiibo_id; /// ID shared by all exact same amiibo. Some amiibo are only distinguished by
                      /// this one like regular SMB Series Mario and the gold one
    u8 type;          /// Type of amiibo, 0: figure, 1: card, 2: plush
    u8 pagex4_byte3;
    u16_le
        appdata_size; /// NFC module writes hard-coded u8 value 0xD8 here. This is the size of the
                      /// amiibo AppData, programs can use this with the AppData R/W commands.
    INSERT_PADDING_BYTES(
        0x30); /// Unused / reserved: this is cleared by NFC module but never written after that.
};

struct AmiiboSettings {
    std::array<u8, 0x60> mii;        /// Owner Mii.
    std::array<u16_le, 11> nickname; /// UTF-16BE Amiibo nickname.
    u8 flags;         /// This is plaintext_amiibosettingsdata[0] & 0xF. See also the NFC_amiiboFlag
                      /// enums.
    u8 countrycodeid; /// This is plaintext_amiibosettingsdata[1]. Country Code ID, from the
                      /// system which setup this amiibo.
    u16_le setupdate_year;
    u8 setupdate_month;
    u8 setupdate_day;
    INSERT_PADDING_BYTES(0x2C); // Normally all-zero?
};

struct IdentificationBlockRaw {
    u16_le char_id;
    u8 char_variant;
    u8 figure_type;
    u16_be model_number;
    u8 series;
    INSERT_PADDING_BYTES(0x2F);
};

struct IdentificationBlockReply {
    u16_le char_id;
    u8 char_variant;
    u8 series;
    u16_le model_number;
    u8 figure_type;
    INSERT_PADDING_BYTES(0x2F);
};

struct AppDataInitStruct {
    std::array<u8, 0xC> data_1;
    u8 data_xc[0x30]; /// The data starting at struct offset 0xC is the 0x30-byte struct used by
                      /// NFC:InitializeWriteAppData, sent by the user-process.
};

struct AppDataWriteStruct {
    std::array<u8, 7> id;
    INSERT_PADDING_BYTES(0x2);
    u8 id_size;
    INSERT_PADDING_BYTES(0x15);
};

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x01, 1, 0};
    Type type{rp.PopEnum<Type>()};
    auto rb{rp.MakeBuilder(1, 0)};
    nfc->type = type;
    nfc->tag_state = TagState::NotScanning;
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "type={}", static_cast<int>(type));
}

void Module::Interface::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x02, 1, 0};
    u8 param{rp.Pop<u8>()};
    nfc->type = Type::Invalid;
    nfc->tag_state = TagState::NotInitialized;
    nfc->encrypted_data.fill(0);
    nfc->decrypted_data.fill(0);
    nfc->amiibo_file.clear();
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "param={}", param);
}

void Module::Interface::StartCommunication(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x03, 1, 0};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "stubbed");
}

void Module::Interface::StopCommunication(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x04, 1, 0};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "stubbed");
}

void Module::Interface::StartTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x05, 1, 0};
    u16 in_val{rp.Pop<u16>()};
    auto rb{rp.MakeBuilder(1, 0)};
    if (nfc->tag_state != TagState::NotScanning && nfc->tag_state != TagState::TagOutOfRange) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    nfc->tag_state = TagState::Scanning;
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "in_val={:04x}", in_val);
}

void Module::Interface::GetTagInfo(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::TagInRange && nfc->tag_state != TagState::TagDataLoaded &&
        nfc->tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x11, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    TagInfo tag_info{};
    tag_info.id_offset_size = 0x7;
    tag_info.unk_x3 = 0x02;
    tag_info.id[0] = nfc->encrypted_data[0];
    tag_info.id[1] = nfc->encrypted_data[1];
    tag_info.id[2] = nfc->encrypted_data[2];
    tag_info.id[3] = nfc->encrypted_data[4];
    tag_info.id[4] = nfc->encrypted_data[5];
    tag_info.id[5] = nfc->encrypted_data[6];
    tag_info.id[6] = nfc->encrypted_data[7];
    IPC::ResponseBuilder rb{ctx, 0x11, (sizeof(TagInfo) / sizeof(u32)) + 1, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<TagInfo>(tag_info);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetAmiiboConfig(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::TagInRange && nfc->tag_state != TagState::TagDataLoaded &&
        nfc->tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x18, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    AmiiboConfig amiibo_config{};
    amiibo_config.lastwritedate_year = AMIIBO_YEAR_FROM_DATE(nfc->decrypted_data[0x32]);
    amiibo_config.lastwritedate_month = AMIIBO_MONTH_FROM_DATE(nfc->decrypted_data[0x32]);
    amiibo_config.lastwritedate_day = AMIIBO_DAY_FROM_DATE(nfc->decrypted_data[0x32]);
    amiibo_config.write_counter = (nfc->decrypted_data[0xB4] << 8) | nfc->decrypted_data[0xB5];
    amiibo_config.character_id[0] = nfc->decrypted_data[0x1DC];
    amiibo_config.character_id[1] = nfc->decrypted_data[0x1DD];
    amiibo_config.character_id[2] = nfc->decrypted_data[0x1DE];
    amiibo_config.series = nfc->decrypted_data[0x1e2];
    amiibo_config.amiibo_id = (nfc->decrypted_data[0x1e0] << 8) | nfc->decrypted_data[0x1e1];
    amiibo_config.type = nfc->decrypted_data[0x1DF];
    amiibo_config.pagex4_byte3 = nfc->decrypted_data[0x2B]; // Raw page 0x4 byte 0x3, decrypted byte
    amiibo_config.appdata_size = 0xD8;
    IPC::ResponseBuilder rb{ctx, 0x18, 17, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<AmiiboConfig>(amiibo_config);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetAmiiboSettings(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::TagInRange && nfc->tag_state != TagState::TagDataLoaded &&
        nfc->tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x17, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    AmiiboSettings amsettings{};
    if (!(nfc->decrypted_data[0x2C] & 0x10)) {
        IPC::ResponseBuilder rb{ctx, 0x17, 1, 0};
        rb.Push(ResultCode(0xC8A17628)); // Uninitialised
    } else {
        std::memcpy(amsettings.mii.data(), &nfc->decrypted_data[0x4C], amsettings.mii.size());
        std::memcpy(amsettings.nickname.data(), &nfc->decrypted_data[0x38],
                    4 * 5); // amiibo doesn't have the null terminator
        amsettings.flags = nfc->decrypted_data[0x2C] &
                           0xF; // TODO: We should only load some of these values if the unused
                                // flag bits are set correctly https://3dbrew.org/wiki/Amiibo
        amsettings.countrycodeid = nfc->decrypted_data[0x2D];
        amsettings.setupdate_year = AMIIBO_YEAR_FROM_DATE(nfc->decrypted_data[0x30]);
        amsettings.setupdate_month = AMIIBO_MONTH_FROM_DATE(nfc->decrypted_data[0x30]);
        amsettings.setupdate_day = AMIIBO_DAY_FROM_DATE(nfc->decrypted_data[0x30]);
    }
    IPC::ResponseBuilder rb{ctx, 0x17, (sizeof(AmiiboSettings) / sizeof(u32)) + 1, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<AmiiboSettings>(amsettings);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::StopTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x06, 1, 0};
    if (nfc->tag_state == TagState::NotInitialized || nfc->tag_state == TagState::NotScanning) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    nfc->tag_state = TagState::NotScanning;
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::LoadAmiiboData(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x07, 1, 0};
    if (nfc->tag_state != TagState::TagInRange) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    if (!amitool_unpack(nfc->encrypted_data.data(), nfc->encrypted_data.size(),
                        nfc->decrypted_data.data(), nfc->decrypted_data.size()))
        LOG_ERROR(Service_NFC, "Failed to decrypt amiibo {}", nfc->amiibo_file);
    else
        nfc->tag_state = TagState::TagDataLoaded;
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::ResetTagScanState(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x08, 1, 0};
    if (nfc->tag_state != TagState::TagDataLoaded && nfc->tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    nfc->tag_state = TagState::TagInRange;
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagInRangeEvent(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::NotScanning) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x0B, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    IPC::ResponseBuilder rb{ctx, 0x0B, 1, 2};
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->tag_in_range_event);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagOutOfRangeEvent(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::NotScanning) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x0C, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    IPC::ResponseBuilder rb{ctx, 0x0C, 1, 2};
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->tag_out_of_range_event);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagState(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x0D, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nfc->tag_state.load());
    LOG_DEBUG(Service_NFC, "tag_state={}", static_cast<int>(nfc->tag_state.load()));
}

void Module::Interface::CommunicationGetStatus(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x0F, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nfc->status);
    LOG_DEBUG(Service_NFC, "status={}", static_cast<int>(nfc->status));
}

void Module::Interface::Unknown0x1A(Kernel::HLERequestContext& ctx) {
    auto result{RESULT_SUCCESS};
    if (nfc->tag_state != TagState::TagInRange) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        result = ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                            ErrorSummary::InvalidState, ErrorLevel::Status);
    } else
        nfc->tag_state = TagState::Unknown6;
    IPC::ResponseBuilder rb{ctx, 0x1A, 1, 0};
    rb.Push(result);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetIdentificationBlock(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::TagDataLoaded && nfc->tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x1B, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    IdentificationBlockRaw identification_block_raw{};
    std::memcpy(&identification_block_raw, &nfc->encrypted_data[0x54], 0x7);
    IdentificationBlockReply identification_block_reply{};
    identification_block_reply.char_id = identification_block_raw.char_id;
    identification_block_reply.char_variant = identification_block_raw.char_variant;
    identification_block_reply.series = identification_block_raw.series;
    identification_block_reply.model_number = identification_block_raw.model_number;
    identification_block_reply.figure_type = identification_block_raw.figure_type;
    IPC::ResponseBuilder rb{ctx, 0x1B, 0x1F, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<IdentificationBlockReply>(identification_block_reply);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::CommunicationGetResult(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x12, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagInfo2(Kernel::HLERequestContext& ctx) {
    if (nfc->tag_state != TagState::TagInRange && nfc->tag_state != TagState::TagDataLoaded &&
        nfc->tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->tag_state.load()));
        IPC::ResponseBuilder rb{ctx, 0x10, 1, 0};
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    TagInfo tag_info{};
    tag_info.id_offset_size = 0x7;
    tag_info.unk_x3 = 0x02;
    tag_info.id[0] = nfc->encrypted_data[0];
    tag_info.id[1] = nfc->encrypted_data[1];
    tag_info.id[2] = nfc->encrypted_data[2];
    tag_info.id[3] = nfc->encrypted_data[4];
    tag_info.id[4] = nfc->encrypted_data[5];
    tag_info.id[5] = nfc->encrypted_data[6];
    tag_info.id[6] = nfc->encrypted_data[7];
    struct {
        TagInfo struct_tag_info;
        INSERT_PADDING_WORDS(1);
        INSERT_PADDING_BYTES(0x30);
    } struct_{};
    struct_.struct_tag_info = std::move(tag_info);
    IPC::ResponseBuilder rb{ctx, 0x10, (sizeof(struct_) / sizeof(u32)) + 1, 0};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(struct_);
    LOG_DEBUG(Service_NFC, "called");
}

Module::Interface::Interface(std::shared_ptr<Module> nfc, const char* name)
    : ServiceFramework{name, 1}, nfc{std::move(nfc)} {}

Module::Interface::~Interface() = default;

void Module::Interface::LoadAmiibo(AmiiboData data, std::string path) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_INFO(Service_NFC, "Loading amiibo {}", path);
    nfc->encrypted_data = data;
    nfc->decrypted_data.fill(0);
    nfc->amiibo_file = path;
    nfc->tag_state = Service::NFC::TagState::TagInRange;
    nfc->tag_in_range_event->Signal();
}

void Module::Interface::RemoveAmiibo() {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_INFO(Service_NFC, "Removing amiibo");
    nfc->encrypted_data.fill(0);
    nfc->decrypted_data.fill(0);
    nfc->amiibo_file.clear();
    nfc->tag_state = Service::NFC::TagState::TagOutOfRange;
    nfc->tag_out_of_range_event->Signal();
}

Module::Module(Core::System& system) : system{system} {
    tag_in_range_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NFC::tag_in_range_event");
    tag_out_of_range_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NFC::tag_out_range_event");
    FileUtil::IOFile keys_file{
        fmt::format("{}amiibo_keys.bin", FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir)),
        "rb"};
    if (!keys_file.IsOpen() || keys_file.GetSize() != 160)
        LOG_ERROR(Service_NFC, "amiibo_keys.bin file not found or invalid.");
    else {
        std::array<u8, 160> keys;
        keys_file.ReadBytes(keys.data(), 160);
        int ret{amitool_setKeys(keys.data(), 160)};
        if (ret != 0)
            LOG_ERROR(Service_NFC, "failed to set keys (error {})", ret);
    }
}

Module::~Module() = default;

void Module::UpdateAmiiboData() {
    FileUtil::IOFile file{amiibo_file, "wb"};
    if (!amitool_pack(decrypted_data.data(), decrypted_data.size(), encrypted_data.data(),
                      encrypted_data.size())) {
        LOG_ERROR(Service_NFC, "Failed to encrypt amiibo");
        return;
    }
    file.WriteBytes(encrypted_data.data(), encrypted_data.size());
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager{system.ServiceManager()};
    auto nfc{std::make_shared<Module>(system)};
    std::make_shared<NFC_M>(nfc)->InstallAsService(service_manager);
    std::make_shared<NFC_U>(nfc)->InstallAsService(service_manager);
}

} // namespace Service::NFC
