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

namespace Keyboard {
    constexpr char KEY_UP    = static_cast<char>(0x80);
    constexpr char KEY_DOWN  = static_cast<char>(0x81);
    constexpr char KEY_LEFT  = static_cast<char>(0x82);
    constexpr char KEY_RIGHT = static_cast<char>(0x83);

    using WakeCallback = void (*)();

    void init();
    void onIRQ();
    bool hasChar();
    char getChar();

    void setFocus(uint32_t task_id);
    uint32_t getFocus();

    void setOnCharCallback(WakeCallback cb);
}