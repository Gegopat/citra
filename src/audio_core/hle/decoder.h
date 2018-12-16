// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/core.h"

namespace AudioCore::HLE {

enum class DecoderCommand : u16 {
    Init,
    Decode,
    Unknown,
};

enum class DecoderCodec : u16 {
    None,
    AAC,
};

struct BinaryRequest {
    enum_le<DecoderCodec> codec{
        DecoderCodec::None}; // This is a guess. until now only 0x1 was observed here
    enum_le<DecoderCommand> cmd{DecoderCommand::Init};
    u32_le fixed{}, src_addr{}, size{}, dst_addr_ch0{}, dst_addr_ch1{}, unknown1{}, unknown2{};
};
static_assert(sizeof(BinaryRequest) == 32, "Unexpected struct size for BinaryRequest");

struct BinaryResponse {
    enum_le<DecoderCodec> codec{
        DecoderCodec::None}; // This could be something else. until now only 0x1 was observed here
    enum_le<DecoderCommand> cmd{DecoderCommand::Init};
    u32_le unknown1{}, unknown2{},
        num_channels{}; // This is a guess, so far we only observed 2 here
    u32_le size{}, unknown3{}, unknown4{},
        num_samples{}; // This is a guess, so far we only observed 1024 here
};
static_assert(sizeof(BinaryResponse) == 32, "Unexpected struct size for BinaryResponse");

class DecoderBase {
public:
    virtual ~DecoderBase();

    virtual std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request) = 0;
};

class NullDecoder final : public DecoderBase {
public:
    NullDecoder();
    ~NullDecoder() override;

    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request) override;
};

} // namespace AudioCore::HLE
