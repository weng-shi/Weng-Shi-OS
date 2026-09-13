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

#include <serial.h>
#include <io.h>

namespace Serial {
    namespace {
        constexpr uint16_t COM1 = 0x3F8;

        constexpr uint16_t REG_DATA = 0x00;
        constexpr uint16_t REG_INT_ENABLE = 0x01;
        constexpr uint16_t REG_FIFO_CTRL = 0x02;
        constexpr uint16_t REG_LINE_CTRL = 0x03;
        constexpr uint16_t REG_MODEM_CTRL = 0x04;
        constexpr uint16_t REG_LINE_STATUS = 0x05;

        bool isTransmitEmpty() {
            return (inb(COM1 + REG_LINE_STATUS) & 0x20) != 0;
        }
    }

    void init() {
        outb(COM1 + REG_INT_ENABLE, 0x00);
        outb(COM1 + REG_LINE_CTRL, 0x80);
        outb(COM1 + REG_DATA, 0x03);
        outb(COM1 + REG_INT_ENABLE, 0x00);
        outb(COM1 + REG_LINE_CTRL, 0x03);
        outb(COM1 + REG_FIFO_CTRL, 0xC7);
        outb(COM1 + REG_MODEM_CTRL, 0x0B);
    }

    void putChar(char c) {
        if (c == '\n') putChar('\r');

        while (!isTransmitEmpty()) {
            asm volatile ("pause");
        }
        outb(COM1 + REG_DATA, static_cast<uint8_t>(c));
    }

    void write(const char *str) {
        for (size_t i = 0; str[i] != '\0'; ++i) putChar(str[i]);
    }

    void writeHex(uint64_t value) {
        write("0x");
        static const char *hex_digits = "0123456789ABCDEF";
        bool leading = true;
        for (int i = 60; i >= 0; i -= 4) {
            uint8_t nibble = (value >> i) & 0xF;
            if (nibble != 0) leading = false;
            if (!leading || i == 0) putChar(hex_digits[nibble]);
        }
    }

    void writeDec(uint64_t value) {
        if (value == 0) {
            putChar('0');
            return;
        }
        char digits[20];
        int count = 0;
        while (value > 0) {
            digits[count++] = '0' + (value % 10);
            value /= 10;
        }
        while (count > 0) {
            putChar(digits[--count]);
        }
    }
}
