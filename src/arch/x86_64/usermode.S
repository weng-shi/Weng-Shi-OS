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

.section .text
.code64

.global jump_to_usermode
jump_to_usermode:
    mov %cx, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    push %rcx
    push %rsi
    pushf
    pop %rax
    or $0x200, %rax
    push %rax
    push %rdx
    push %rdi

    iretq

.global jump_to_usermode_args
jump_to_usermode_args:
    mov %cx, %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs

        push %rcx
        push %rsi
        pushf
        pop %rax
        or $0x200, %rax
        push %rax
        push %rdx
        push %rdi

        mov %r8, %rdi
        mov %r9, %rsi

        iretq