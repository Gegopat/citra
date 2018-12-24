# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Crypto++
add_subdirectory(cryptopp)

# Dynarmic
# Dynarmic will skip defining xbyak if it's already defined, we then define it below
add_library(xbyak INTERFACE)
option(DYNARMIC_TESTS OFF)
set(DYNARMIC_NO_BUNDLED_FMT ON)
add_subdirectory(dynarmic)

# libfmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)

# Glad
add_subdirectory(glad)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)

# Open Source Archives
add_subdirectory(open_source_archives)

# SoundTouch
add_subdirectory(soundtouch)
# The SoundTouch target doesn't export the necessary include paths as properties by default
target_include_directories(SoundTouch INTERFACE ./soundtouch/include)

# Teakra
add_subdirectory(teakra)

# Xbyak
# Defined before "dynarmic" above
# add_library(xbyak INTERFACE)
target_include_directories(xbyak INTERFACE ./xbyak/xbyak)
target_compile_definitions(xbyak INTERFACE XBYAK_NO_OP_NAMES)

# ENet
add_subdirectory(enet)
target_include_directories(enet INTERFACE ./enet/include)

if (ENABLE_DISCORD_RPC)
    # Discord RPC
    set(BUILD_EXAMPLES OFF CACHE BOOL "")
    add_subdirectory(discord-rpc EXCLUDE_FROM_ALL)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
endif()

# mbedtls
set(ENABLE_PROGRAMS OFF CACHE BOOL "")
set(ENABLE_TESTING OFF CACHE BOOL "")
add_subdirectory(mbedtls)
target_include_directories(mbedtls PUBLIC ./mbedtls/include)

# asl
set(ASL_TLS ON CACHE BOOL "")
add_subdirectory(asl EXCLUDE_FROM_ALL)

# Cubeb
set(BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(cubeb EXCLUDE_FROM_ALL)

# amitool
add_subdirectory(amitool)
