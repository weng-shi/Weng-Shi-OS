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
#include <colors.h>

class Terminal {
public:
    Terminal();

    void clear();
    void clearAll();
    void putChar(char c);
    void putCharAt(char c, size_t x, size_t y);
    void write(const char* str);
    void writeAt(const char *str, size_t x, size_t y);
    void writeHex(uint64_t value);
    void writeDec(uint64_t value);
    void writeDecAt(uint64_t value, size_t x, size_t y);
    void setColor(uint8_t fg, uint8_t bg);
    void setColorForeground(uint8_t fg);
    void setColorBackground(uint8_t bg);
    void backspace();
    [[nodiscard]] uint16_t getRow() const { return row_; }
    [[nodiscard]] uint16_t getColumn() const { return column_; }
    void setCursor(uint16_t row, uint16_t col);

    void enableHardwareCursor();
    void updateHardwareCursor();

    void newline();

private:

    void scroll();

    static constexpr uint16_t VGA_WIDTH  = 80;
    static constexpr uint16_t VGA_HEIGHT = 24;

    volatile uint16_t* const buffer_;
    uint16_t row_;
    uint16_t column_;
    uint8_t color_;

    static constexpr uint16_t vgaEntry(char c, uint8_t color) {
        return static_cast<uint16_t>(static_cast<uint8_t>(c)) | (static_cast<uint16_t>(color) << 8);
    }
};

extern Terminal g_terminal;