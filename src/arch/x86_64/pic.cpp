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

#include <pic.h>
#include <io.h>

namespace {
    constexpr uint16_t PIC1_COMMAND =   0x20;
    constexpr uint16_t PIC1_DATA =      0x21;
    constexpr uint16_t PIC2_COMMAND =   0xA0;
    constexpr uint16_t PIC2_DATA =      0xA1;

    constexpr uint8_t ICW1_INIT =       0x10;
    constexpr uint8_t ICW1_ICW4 =       0x01;
    constexpr uint8_t ICW4_8086 =       0x01;
    constexpr uint8_t PIC_EOI =         0x20;
}

namespace PIC {
    void remap(uint8_t master_offset, uint8_t slave_offset) {
        uint8_t mask1 = inb(PIC1_DATA);
        uint8_t mask2 = inb(PIC2_DATA);

        outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();
        outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();

        outb(PIC1_DATA, master_offset); io_wait();
        outb(PIC2_DATA, slave_offset); io_wait();

        outb(PIC1_DATA, 4); io_wait();
        outb(PIC2_DATA, 2); io_wait();

        outb(PIC1_DATA, ICW4_8086); io_wait();
        outb(PIC2_DATA, ICW4_8086); io_wait();

        outb(PIC1_DATA, mask1);
        outb(PIC2_DATA, mask2);
    }

    void sendEOI(uint8_t irq) {
        if (irq >= 8) outb(PIC2_COMMAND, PIC_EOI);
        outb(PIC1_COMMAND,PIC_EOI); io_wait();
    }

    void setMask(uint8_t irq) {
        uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
        uint8_t irq_line = (irq < 8) ? irq : irq - 8;
        uint8_t value = inb(port) | (1 << irq_line);
        outb(port, value);
    }

    void clearMask(uint8_t irq) {
        uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
        uint8_t irq_line = (irq < 8) ? irq : irq - 8;
        uint8_t value = inb(port) & ~(1 << irq_line);
        outb(port, value);
    }
}
