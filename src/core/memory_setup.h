// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/mmio.h"

namespace Memory {

class MemorySystem;

/**
 * Maps an allocated buffer onto a region of the emulated process address space.
 *
 * @param page_table The page table of the emulated process.
 * @param base The address to start mapping at. Must be page-aligned.
 * @param size The amount of bytes to map. Must be page-aligned.
 * @param target Buffer with the memory backing the mapping. Must be of length at least `size`.
 */
void MapMemoryRegion(MemorySystem& memory, PageTable& page_table, VAddr base, u32 size, u8* target);

/**
 * Maps a region of the emulated process address space as a IO region.
 * @param page_table The page table of the emulated process.
 * @param base The address to start mapping at. Must be page-aligned.
 * @param size The amount of bytes to map. Must be page-aligned.
 * @param mmio_handler The handler that backs the mapping.
 */
void MapIoRegion(MemorySystem& memory, PageTable& page_table, VAddr base, u32 size,
                 MMIORegionPointer mmio_handler);

void UnmapRegion(MemorySystem& memory, PageTable& page_table, VAddr base, u32 size);

} // namespace Memory
