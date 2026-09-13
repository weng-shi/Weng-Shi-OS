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

#include <power.h>
#include <io.h>
#include <terminal.h>

namespace Power {
    [[noreturn]] void halt() {
        while (true) {
            asm volatile("hlt");
        }
    }

    void reboot() {
        uint8_t good = 0x02;
        while (good & 0x02)
            good = inb(0x64);
        outb(0x64, 0xFE);
        halt();
    }

    void rebootResetVector() {
        // Not supported in long mode
    }
}
