// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/spi/spi.h"

namespace Service::SPI {

class SPI_DEF final : public Module::Interface {
public:
    explicit SPI_DEF(std::shared_ptr<Module> spi);
};

} // namespace Service::SPI
