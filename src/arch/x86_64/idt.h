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

struct Registers {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t vector, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

extern "C" uint64_t g_next_task_rsp;

using IrqHandler = void (*)();

void idt_init();
void idt_register_irq_handler(uint8_t irq, IrqHandler handler);
void idt_register_syscall_handler();