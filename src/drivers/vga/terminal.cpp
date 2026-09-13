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

#include <terminal.h>
#include <io.h>

Terminal::Terminal()
    : buffer_(reinterpret_cast<uint16_t *>(0xB8000)),
      row_(0), column_(0),
      color_(0x0F)
{
    clear();
}

void Terminal::setColor(uint8_t fg, uint8_t bg) {
    color_ = fg | (bg << 4);
}

void Terminal::setColorForeground(uint8_t fg) {
    uint8_t bg = (color_ & 0xF0) >> 4;
    g_terminal.setColor(fg, bg);
}

void Terminal::setColorBackground(uint8_t bg) {
    uint8_t fg = color_ & 0x0F;
    g_terminal.setColor(fg, bg);
}

void Terminal::clear() {
    for (uint16_t y = 0; y < VGA_HEIGHT; ++y)
        for (uint16_t x = 0; x < VGA_WIDTH; ++x)
            buffer_[y * VGA_WIDTH + x] = vgaEntry(' ', color_);
    row_ = 0;
    column_ = 0;
    updateHardwareCursor();
}

void Terminal::clearAll() {
    for (uint16_t y = 0; y < VGA_HEIGHT + 1; ++y)
        for (uint16_t x = 0; x < VGA_WIDTH; ++x)
            buffer_[y * VGA_WIDTH + x] = vgaEntry(' ', color_);
    row_ = 0;
    column_ = 0;
    updateHardwareCursor();
}

void Terminal::newline() {
    column_ = 0;
    if (row_ == VGA_HEIGHT - 1) {
        scroll();
    } else {
        ++row_;
    }
}

void Terminal::putChar(char c) {
    if (c == '\n') {
        newline();
        return;
    }
    buffer_[row_ * VGA_WIDTH + column_] = vgaEntry(c, color_);
    if (++column_ == VGA_WIDTH) newline();
    updateHardwareCursor();
}

void Terminal::putCharAt(char c, size_t x, size_t y) {
    buffer_[y * VGA_WIDTH + x] = vgaEntry(c, color_);
}

void Terminal::setCursor(uint16_t row, uint16_t col) {
    row_ = row;
    column_ = col;
    updateHardwareCursor();
}

void Terminal::scroll() {
    for (uint16_t y = 1; y < VGA_HEIGHT; ++y) {
        for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
            buffer_[(y - 1) * VGA_WIDTH + x] = buffer_[y * VGA_WIDTH + x];
        }
    }

    for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
        buffer_[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vgaEntry(' ', color_);
    }
}

void Terminal::write(const char *str) {
    for (size_t i = 0; str[i] != '\0'; ++i) putChar(str[i]);
}

void Terminal::writeAt(const char *str, size_t x, size_t y) {
    for (size_t i = 0; str[i] != '\0'; ++i) putCharAt(str[i], x + i, y);
}

void Terminal::writeHex(uint64_t value) {
    write("0x");
    static const char *hex_digits = "0123456789ABCDEF";
    bool leading = true;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        if (nibble != 0) leading = false;
        if (!leading || i == 0) putChar(hex_digits[nibble]);
    }
}

void Terminal::backspace() {
    if (column_ > 0) {
        --column_;
    } else if (row_ > 0) {
        --row_;
        column_ = VGA_WIDTH - 1;
    } else {
        return;
    }
    buffer_[row_ * VGA_WIDTH + column_] = vgaEntry(' ', color_);
    updateHardwareCursor();
}

void Terminal::enableHardwareCursor() {
    outb(0x3D4, 0x0A);
    uint8_t cursor_start = inb(0x3D5);
    outb(0x3D5, (cursor_start & 0xC0) | 14);

    outb(0x3D4, 0x0B);
    uint8_t cursor_end = inb(0x3D5);
    outb(0x3D5, (cursor_end & 0xE0) | 15);
}

void Terminal::updateHardwareCursor() {
    uint16_t position = row_ * VGA_WIDTH + column_;

    outb(0x3D4, 0x0F);
    outb(0x3D5, static_cast<uint8_t>(position & 0xFF));

    outb(0x3D4, 0x0E);
    outb(0x3D5, static_cast<uint8_t>((position >> 8) & 0xFF));
}

void Terminal::writeDec(uint64_t value) {
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

void Terminal::writeDecAt(uint64_t value, size_t x, size_t y) {
    if (value == 0) {
        putCharAt('0', x, y);
        return;
    }
    char digits[20];
    int count = 0;
    while (value > 0) {
        digits[count++] = '0' + (value % 10);
        value /= 10;
    }
    for (size_t i = 0; count > 0; ++i) {
        putCharAt(digits[--count], x + i, y);
    }
}

Terminal g_terminal;
