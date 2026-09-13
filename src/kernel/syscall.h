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
#include <idt.h>

constexpr uint64_t SYS_WRITE = 0;
constexpr uint64_t SYS_EXIT  = 1;
constexpr uint64_t SYS_READ  = 2;
constexpr uint64_t SYS_OPEN  = 3;
constexpr uint64_t SYS_CLOSE  = 4;
constexpr uint64_t SYS_BRK  = 5;
constexpr uint64_t SYS_READDIR = 6;
constexpr uint64_t SYS_EXEC = 7;
constexpr uint64_t SYS_WAIT = 8;
constexpr uint64_t SYS_SPAWN = 9;

extern "C" void syscall_dispatch(Registers* regs);