// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Directory separators
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

// The user directory
#define ROOT_DIR "."
#define USER_DIR "user"

#ifdef _WIN32
#define DATA_DIR "Citra"
#else
#define DATA_DIR "citra-emu"
#endif

// Subdirs in the user directory returned by GetUserPath(UserPath::UserDir)
#define CONFIG_DIR "config"
#define SDMC_DIR "sdmc"
#define NAND_DIR "nand"
#define SYSDATA_DIR "sysdata"
#define CHEATS_DIR "cheats"
#define DLL_DIR "external_dlls"

// Filenames
#define LOG_FILE "log.txt"

// System files
#define AES_KEYS "aes_keys.txt"
#define BOOTROM9 "boot9.bin"
#define SECRET_SECTOR "sector0x96.bin"
