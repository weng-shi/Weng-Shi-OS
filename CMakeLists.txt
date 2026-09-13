#
# Copyright (c) 2026 WengShi
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchain-x86_64-elf.cmake)

project(WengShiOS ASM C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FREESTANDING_FLAGS
        -ffreestanding
        -fno-builtin
        -fno-stack-protector
        -fno-stack-check
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mcmodel=kernel
        -mno-sse
        -mno-sse2
        -mno-ssse3
        -mno-sse4.1
        -mno-sse4.2
        -mno-avx
        -mno-avx2
        -mno-fma
        -mno-aes
        -mno-pclmul
        -mno-popcnt
        -mno-bmi
        -mno-mmx
        -mno-80387
        -mgeneral-regs-only
        -Wall -Wextra

)

set(CXX_ONLY_FLAGS
        -fno-exceptions
        -fno-rtti
        -fno-unwind-tables
)

add_compile_options(${FREESTANDING_FLAGS})
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_ONLY_FLAGS}>")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -ffreestanding")

include_directories(
        src/kernel
        src/arch/x86_64
        src/mm
        src/drivers/vga
        src/drivers/keyboard
        src/drivers/pci
        src/drivers/power
        src/drivers/cmos
        src/drivers/serial
        src/drivers/ahci
        src/libc
        src/include
        src/shell
        src/fs
        src/fs/ext2
        src/shed
        src/file_formats/elf
        src/file_formats/tar
)

add_executable(kernel.bin
        src/boot/boot.S # multiboot GRUB boot file

        src/kernel/main.cpp # main file
        src/kernel/panic.cpp # CPU exception handling
        src/kernel/multiboot.cpp # multiboot2 table parsing
        src/kernel/syscall.cpp

        # GDT/IDT tables + timer
        src/arch/x86_64/gdt.cpp
        src/arch/x86_64/gdt_flush.S
        src/arch/x86_64/idt.cpp
        src/arch/x86_64/isr.S
        src/arch/x86_64/timer.cpp
        src/arch/x86_64/usermode.cpp
        src/arch/x86_64/usermode.S
        src/arch/x86_64/syscall.S

        # low level communication
        src/arch/x86_64/io.cpp
        src/arch/x86_64/pic.cpp

        # memory management
        src/mm/pmm.cpp
        src/mm/vmm.cpp
        src/mm/heap.cpp
        src/mm/new_delete.cpp

        # display input/output
        src/drivers/vga/terminal.cpp
        src/drivers/vga/statusbar.cpp
        src/drivers/vga/colors.h

        # keyboard input
        src/drivers/keyboard/keyboard.cpp

        # pci enumeration
        src/drivers/pci/pci.cpp

        # power handling
        src/drivers/power/power.cpp

        # CMOS and RTC reading
        src/drivers/cmos/cmos.cpp
        src/drivers/cmos/rtc.cpp

        # serial UART communication
        src/drivers/serial/serial.cpp

        # AHCI io
        src/drivers/ahci/ahci.cpp

        # libc
        src/libc/string.cpp
        src/libc/stdlib.cpp
        src/libc/printf.cpp

        # shell
        src/shell/shell.cpp

        src/fs/vfs.cpp

        src/fs/ext2/ext2.cpp

        src/shed/scheduler.cpp

        src/file_formats/elf/elf.cpp
        src/file_formats/tar/tar.cpp
)



target_link_options(kernel.bin PRIVATE
        -ffreestanding
        -nostdlib
        -static
        -no-pie
        -Wl,-n
        -T ${CMAKE_SOURCE_DIR}/linker.ld
)

set_target_properties(kernel.bin PROPERTIES
        LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld
        SUFFIX ""
)

set(ISO_DIR ${CMAKE_BINARY_DIR}/isodir)
set(ISO_FILE ${CMAKE_BINARY_DIR}/WengShiOS.iso)

add_custom_command(
        OUTPUT ${ISO_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.bin> ${ISO_DIR}/boot/kernel.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub/grub.cfg ${ISO_DIR}/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o ${ISO_FILE} ${ISO_DIR}
        DEPENDS kernel.bin ${CMAKE_SOURCE_DIR}/grub/grub.cfg
        COMMENT "Building GRUB bootable iso file"
)

add_custom_target(iso ALL DEPENDS ${ISO_FILE})

set(QEMU_DRIVES_FLAGS
        -device ich9-ahci,id=ahci
        -drive file=disk.img,if=none,id=disk1,format=raw
        -device ide-hd,bus=ahci.0,drive=disk1
        -drive file=disk2.img,if=none,id=disk2,format=raw
        -device ide-hd,bus=ahci.1,drive=disk2
)

add_custom_target(run
        COMMAND qemu-system-x86_64 -cdrom ${ISO_FILE} -serial stdio -m 512M -cpu qemu64 -machine q35 ${QEMU_DRIVES_FLAGS} -vga std --enable-kvm
        DEPENDS iso
        COMMENT "Running kernel in QEMU"
)

add_custom_target(debug
        COMMAND qemu-system-x86_64 -cdrom ${ISO_FILE} -serial stdio -m 32M -s -S
        DEPENDS iso
        COMMENT "Running kernel in QEMU, debug mode, listening for GDB on localhost:1234"
)
