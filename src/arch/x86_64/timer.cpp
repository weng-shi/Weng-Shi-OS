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

#include <timer.h>
#include <io.h>
#include <idt.h>

namespace Timer {
    namespace {
        constexpr uint16_t PIT_CHANNEL0_DATA = 0x40;
        constexpr uint16_t PIT_COMMAND = 0x43;
        constexpr uint32_t PIT_BASE_FREQUENCY = 1193182;

        volatile uint64_t g_ticks = 0;
        uint32_t g_frequency_hz = 0;
    }

    void init(uint32_t frequency_hz) {
        g_frequency_hz = frequency_hz;
        g_ticks = 0;
        uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;
        if (divisor > 0xFFFF) divisor = 0xFFFF;

        outb(PIT_COMMAND, 0x36);
        outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
        outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);

        idt_register_irq_handler(0, onIRQ);
    }

    uint64_t getTicks() {
        return g_ticks;
    }

    void onIRQ() {
        ++g_ticks;
    }

    void sleepTicks(uint64_t ticks) {
        const uint64_t ticks_ = getTicks();
        while (true) {
            if ((ticks_ + ticks) < getTicks()) break;
        }
    }
    void sleepMiliseconds(uint64_t ms) {
        const uint64_t ticks_ = getTicks();
        while (true) {
            if ((ticks_ + ms) < getTicks()) break;
        }
    }

    void sleepSeconds(uint64_t s) {
        const uint64_t ticks_ = getTicks();
        while (true) {
            if ((ticks_ + s * 1000) < getTicks()) break;
        }
    }
}
