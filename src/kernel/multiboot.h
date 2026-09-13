/*
* Copyright (c) 2026 WengShi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#include <stdint.h>

namespace Multiboot {
    struct TagHeader {
        uint32_t type;
        uint32_t size;
    } __attribute__((packed));

    constexpr uint32_t TAG_TYPE_END     = 0;
    constexpr uint32_t TAG_TYPE_MMAP    = 6;

    struct MmapEntry {
        uint64_t base_addr;
        uint64_t length;
        uint32_t type;
        uint32_t reserved;
    } __attribute__((packed));

    constexpr uint32_t MEMORY_AVAILABLE             = 1;
    constexpr uint32_t MEMORY_RESERVED              = 2;
    constexpr uint32_t MEMORY_ACPI_RECLAIMABLE      = 3;
    constexpr uint32_t MEMORY_NVS                   = 4;
    constexpr uint32_t MEMORY_BADRAM                = 5;

    struct MmapTag {
        TagHeader header;
        uint32_t entry_size;
        uint32_t entry_version;
        MmapEntry entries[];
    } __attribute__((packed));

    using MmapCallback = void (*)(const MmapEntry &entry, void *user_data);

    bool parseMemoryMap(uint32_t mb_info_addr, MmapCallback callback, void *user_data = nullptr);

    const char *memoryTypeToString(uint32_t type);

}