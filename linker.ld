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

ENTRY(_start)

KERNEL_PHYSICAL_BASE = 0x100000;
KERNEL_VIRTUAL_BASE  = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KERNEL_PHYSICAL_BASE;

    .boot ALIGN(4K) :
    {
        KEEP(*(.multiboot))
        *(.boot.text)
        *(.boot.data)
        *(.boot.bss)
    }

    . += KERNEL_VIRTUAL_BASE;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
    {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE)
    {
        *(.rodata .rodata.*)
    }

    .ctors ALIGN(8) : AT(ADDR(.ctors) - KERNEL_VIRTUAL_BASE)
    {
        PROVIDE_HIDDEN(__ctors_start = .);
        KEEP(*(.ctors))
        KEEP(*(.ctors.*))
        PROVIDE_HIDDEN(__ctors_end = .);
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE)
    {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    . = ALIGN(4K);
    kernel_end = .;
    kernel_physical_end = kernel_end - KERNEL_VIRTUAL_BASE;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}