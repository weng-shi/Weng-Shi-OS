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

#include <keyboard.h>
#include <io.h>
#include <terminal.h>

#include "scheduler.h"

namespace Keyboard {
    namespace {
        constexpr uint16_t DATA_PORT = 0x60;
        constexpr size_t BUFFER_SIZE = 256;
        char g_buffer[BUFFER_SIZE];
        size_t g_head = 0;
        size_t g_tail = 0;

        bool g_shift_pressed = false;
        bool g_extended = false;

        const char scancode_ascii[128] = {
            0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
            '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
            0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
            0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
            '*', 0, ' ', 0,
        };

        const char scancode_ascii_shift[128] = {
            0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
            '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
            0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
            0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
            '*', 0, ' ', 0,
        };

        constexpr uint8_t SCANCODE_LSHIFT_DOWN = 0x2A;
        constexpr uint8_t SCANCODE_RSHIFT_DOWN = 0x36;
        constexpr uint8_t SCANCODE_LSHIFT_UP = 0xAA;
        constexpr uint8_t SCANCODE_RSHIFT_UP = 0xB6;
        constexpr uint8_t SCANCODE_RELEASE_BIT = 0x80;

        constexpr uint8_t SCANCODE_EXT_UP = 0x48;
        constexpr uint8_t SCANCODE_EXT_DOWN = 0x50;
        constexpr uint8_t SCANCODE_EXT_LEFT = 0x4B;
        constexpr uint8_t SCANCODE_EXT_RIGHT = 0x4D;

        void pushChar(char c) {
            size_t next = (g_head + 1) % BUFFER_SIZE;
            if (next == g_tail) return;
            g_buffer[g_head] = c;
            g_head = next;
        }

        uint32_t g_focused_task_id = 0;
        Keyboard::WakeCallback g_on_char_callback = nullptr;

    }

    void setFocus(uint32_t task_id) {
        g_focused_task_id = task_id;
    }

    uint32_t getFocus() {
        return g_focused_task_id;
    }

    namespace {
        bool callerIsFocused() {
            if (g_focused_task_id == 0) return true;
            Scheduler::Task* current = Scheduler::currentTask();
            return current && current->id == g_focused_task_id;
        }
    }


    void setOnCharCallback(WakeCallback cb) {
        g_on_char_callback = cb;
    }

    void init() {
        g_head = 0;
        g_tail = 0;
        g_shift_pressed = false;
        g_extended = false;
    }

    void onIRQ() {
        uint8_t scancode = inb(DATA_PORT);

        if (scancode == 0xE0) {
            g_extended = true;
            return;
        }

        if (g_extended) {
            g_extended = false;

            if (scancode & SCANCODE_RELEASE_BIT) return;

            switch (scancode) {
                case SCANCODE_EXT_UP: pushChar(KEY_UP);
                    break;
                case SCANCODE_EXT_DOWN: pushChar(KEY_DOWN);
                    break;
                case SCANCODE_EXT_LEFT: pushChar(KEY_LEFT);
                    break;
                case SCANCODE_EXT_RIGHT: pushChar(KEY_RIGHT);
                    break;
                default: break;
            }
            return;
        }

        if (scancode == SCANCODE_LSHIFT_DOWN || scancode == SCANCODE_RSHIFT_DOWN) {
            g_shift_pressed = true;
            return;
        }
        if (scancode == SCANCODE_LSHIFT_UP || scancode == SCANCODE_RSHIFT_UP) {
            g_shift_pressed = false;
            return;
        }

        if (scancode & SCANCODE_RELEASE_BIT) return;
        if (scancode >= 128) return;

        char c = g_shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
        if (c != 0) {
            pushChar(c);
            if (g_on_char_callback) {
                WakeCallback cb = g_on_char_callback;
                g_on_char_callback = nullptr;
                cb();
            }
        }
    }

    bool hasChar() {
        return g_head != g_tail && callerIsFocused();
    }

    char getChar() {
        if (g_head == g_tail || !callerIsFocused()) return 0;
        char c = g_buffer[g_tail];
        g_tail = (g_tail + 1) % BUFFER_SIZE;
        return c;
    }
}
