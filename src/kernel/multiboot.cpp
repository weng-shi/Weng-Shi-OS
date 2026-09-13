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

#include <multiboot.h>

namespace Multiboot {
    const char *memoryTypeToString(uint32_t type) {
        switch (type) {
            case MEMORY_AVAILABLE:          return "Available";
            case MEMORY_RESERVED:           return "Reserved";
            case MEMORY_ACPI_RECLAIMABLE:   return "ACPI Reclaimable";
            case MEMORY_NVS:                return "ACPI NVS";
            case MEMORY_BADRAM:             return "Bad RAM";
            default:                        return "Unknown";
        }
    }

    bool parseMemoryMap(uint32_t mb_info_addr, MmapCallback callback, void *user_data) {
        const uint8_t *base = reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(mb_info_addr));
        uint32_t total_size = *reinterpret_cast<const uint32_t *>(base);

        const uint8_t *ptr = base + 8;
        const uint8_t *end = base + total_size;

        bool found_mmap = false;

        while (ptr < end) {
            const TagHeader *tag = reinterpret_cast<const TagHeader *>(ptr);

            if (tag->type == TAG_TYPE_END) break;

            if (tag->type == TAG_TYPE_MMAP) {
                found_mmap = true;

                const MmapTag *mmap_tag = reinterpret_cast<const MmapTag *>(tag);

                uint32_t entry_count = (mmap_tag->header.size - sizeof(MmapTag)) / mmap_tag->entry_size;

                const uint8_t *entry_ptr = reinterpret_cast<const uint8_t *>(mmap_tag->entries);

                for (uint32_t i = 0; i < entry_count; ++i) {
                    const MmapEntry *entry = reinterpret_cast<const MmapEntry *>(entry_ptr);
                    callback(*entry, user_data);
                    entry_ptr += mmap_tag->entry_size;
                }
            }
            ptr += (tag->size + 7) & ~7u;
        }
        return found_mmap;
    }
}
