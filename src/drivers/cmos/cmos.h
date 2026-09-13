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
#include <stddef.h>

namespace CMOS {
    constexpr uint8_t CMOS_PORT_INDEX = 0x70;
    constexpr uint8_t CMOS_PORT_DATA = 0x71;

    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);

}
