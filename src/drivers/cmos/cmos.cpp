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

#include <cmos.h>
#include <io.h>

namespace CMOS {
    uint8_t readRegister(const uint8_t reg) {
        asm volatile ("cli");
        outb(CMOS_PORT_INDEX, reg | 0x80);
        uint8_t result = inb(CMOS_PORT_DATA);
        asm volatile ("sti");
        return result;
    }

    void writeRegister(const uint8_t reg, const uint8_t value) {
        outb(CMOS_PORT_DATA, reg | 0x80);
        outb(CMOS_PORT_INDEX, value);
    }
}
