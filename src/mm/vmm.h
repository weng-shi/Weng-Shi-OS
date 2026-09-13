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

namespace VMM {

    constexpr uint64_t PAGE_PRESENT  = 1ull << 0;
    constexpr uint64_t PAGE_WRITABLE = 1ull << 1;
    constexpr uint64_t PAGE_USER     = 1ull << 2;
    constexpr uint64_t PAGE_CACHE_DISABLE = 1ull << 4;
    constexpr uint64_t PAGE_SIZE     = 4096;

    void init();

    void mapPage(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);

    void unmapPage(uint64_t virtual_addr);

    bool allocAndMapPage(uint64_t virtual_addr, uint64_t flags);

    uint64_t createAddressSpace();

    void switchAddressSpace(uint64_t plm4_phys);

    uint64_t currentAddressSpace();

}