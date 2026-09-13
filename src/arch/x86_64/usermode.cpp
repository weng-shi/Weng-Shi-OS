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

#include <usermode.h>
#include <gdt.h>

extern "C" void jump_to_usermode_args(uint64_t entry, uint64_t stack, uint64_t user_cs, uint64_t user_ss,
                                        uint64_t argc, uint64_t argv);

namespace Usermode {
    [[noreturn]] void enter(uint64_t entry_point, uint64_t user_stack, int argc, uint64_t argv_user_ptr) {
        jump_to_usermode_args(entry_point, user_stack, GDT_USER_CODE, GDT_USER_DATA, argc, argv_user_ptr);
        for (;;) {}
    }
}