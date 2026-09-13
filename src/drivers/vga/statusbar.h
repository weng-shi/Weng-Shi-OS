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
#include <stddef.h>

namespace StatusBar {
    constexpr size_t STATUS_BAR_POSITION_END = 79;
    constexpr size_t STATUS_BAR_Y = 24;

    [[noreturn]]void date();
    [[noreturn]]void time();
}