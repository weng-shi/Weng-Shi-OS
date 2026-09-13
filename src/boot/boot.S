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

.section .multiboot, "a"
.align 8

multiboot_header:
    .long 0xe85250d6
    .long 0
    .long multiboot_header_end - multiboot_header
    .long -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header))

    .word 0
    .word 0
    .long 8
multiboot_header_end:

.section .boot.bss, "aw", @nobits
.align 4096
p4_table:      .skip 4096
p3_table_lo:   .skip 4096
p3_table_hi:   .skip 4096
p2_table:      .skip 4096
.align 16
stack_bottom:  .skip 16384
stack_top:

.section .boot.text, "ax"
.code32
.global _start
.type _start, @function

_start:
    cli
    mov $stack_top, %esp
    mov %esp, %ebp

    mov %ebx, multiboot_info_ptr

    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    lgdt gdt64_pointer
    ljmp $0x08, $long_mode_start

hang:
    hlt
    jmp hang

check_cpuid:
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $(1 << 21), %eax
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    cmp %ecx, %eax
    je no_cpuid
    ret
no_cpuid:
    jmp hang

check_long_mode:
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb no_long_mode

    mov $0x80000001, %eax
    cpuid
    test $(1 << 29), %edx
    jz no_long_mode
    ret
no_long_mode:
    jmp hang

setup_page_tables:
    mov $p3_table_lo, %eax
    or $0b111, %eax
    mov %eax, p4_table

    mov $p3_table_hi, %eax
    or $0b11, %eax
    mov %eax, p4_table + 511*8

    mov $p4_table, %eax
    or $0b11, %eax
    mov %eax, p4_table + 510*8

    mov $p2_table, %eax
    or $0b111, %eax
    mov %eax, p3_table_lo

    mov $p2_table, %eax
    or $0b11, %eax
    mov %eax, p3_table_hi + 510*8

    mov $0, %ecx
map_p2_table:
    mov $0x200000, %eax
    mul %ecx
    or $0b10000011, %eax
    mov %eax, p2_table(, %ecx, 8)

    inc %ecx
    cmp $512, %ecx
    jne map_p2_table
    ret

enable_paging:
    mov $p4_table, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0
    ret

.section .boot.data, "aw"
.align 8
gdt64:
    .quad 0
gdt64_code:
    .quad (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

.global multiboot_info_ptr
multiboot_info_ptr: .long 0

.global p4_table_phys_addr
p4_table_phys_addr: .quad p4_table

.section .boot.text
.code64
long_mode_start:
    mov $0, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov multiboot_info_ptr(%rip), %edi

    movabs $kernel_main, %rax
    call *%rax

    cli
.hlt_loop:
    hlt
    jmp .hlt_loop