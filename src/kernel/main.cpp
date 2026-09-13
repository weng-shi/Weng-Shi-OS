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

#include <stdint.h>
#include <terminal.h>
#include <gdt.h>
#include <idt.h>
#include <multiboot.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <pic.h>
#include <keyboard.h>
#include <shell.h>
#include <io.h>
#include <pci.h>
#include <rtc.h>
#include <serial.h>
#include <ahci.h>
#include <timer.h>
#include <scheduler.h>
#include <statusbar.h>
#include <string.h>

#include "usermode.h"
#include <printf.h>
#include <elf.h>

extern "C" {
typedef void (*ctor_func_t)();
extern ctor_func_t __ctors_start[];
extern ctor_func_t __ctors_end[];

void call_global_constructors() {
    for (ctor_func_t* ctor = __ctors_end; ctor != __ctors_start; ) {
        --ctor;
        (*ctor)();
    }
}
}

extern "C" uint8_t kernel_physical_end[];

namespace {
    uint64_t g_highest_address = 0;

    void findHighestAddress(const Multiboot::MmapEntry &entry, void *) {
        if (entry.type == Multiboot::MEMORY_AVAILABLE) {
            uint64_t region_end = entry.base_addr + entry.length;
            if (region_end > g_highest_address) {
                g_highest_address = region_end;
            }
        }
    }

    void freeAvailableRegions(const Multiboot::MmapEntry &entry, void *) {
        if (entry.type == Multiboot::MEMORY_AVAILABLE) {
            g_pmm.markRegionFree(entry.base_addr, entry.length);
        }
    }

}

extern "C" [[noreturn]] void kernel_main(uint32_t multiboot_info_ptr) {
    Serial::init();
    Serial::write("=====WengShi OS=====\n");
    call_global_constructors();
    (void)multiboot_info_ptr;

    printf("%r===================================WengShi OS===================================%r\n", VGA::BLUE, VGA::WHITE);

    g_terminal.enableHardwareCursor();
    Serial::write("VGA screen initialized\n");

    gdt_init();
    printf("%rGDT initialized.%r\n", VGA::GREEN, VGA::WHITE);

    idt_init();
    printf("%rIDT initialized.%r\n", VGA::GREEN, VGA::WHITE);

    PIC::remap(0x20, 0x28);
    printf("%rPIC initialized.%r\n", VGA::GREEN, VGA::WHITE);

    Keyboard::init();
    idt_register_irq_handler(1, Keyboard::onIRQ);
    PIC::clearMask(1);
    printf("%rKeyboard initialized.%r\n", VGA::GREEN, VGA::WHITE);
    Timer::init(1000);
    PIC::clearMask(0);
    printf("%rPIT initialized.%r\n", VGA::GREEN, VGA::WHITE);

    asm volatile("sti");
    printf("%rInterrupts initialized.%r\n", VGA::GREEN, VGA::WHITE);

    Multiboot::parseMemoryMap(multiboot_info_ptr, findHighestAddress);

    uint64_t kernel_end_addr = reinterpret_cast<uint64_t>(kernel_physical_end);
    uint64_t bitmap_addr = (kernel_end_addr + 0xFFF) & ~0xFFFull;
    uint8_t* bitmap_ptr = reinterpret_cast<uint8_t*>(bitmap_addr);

    g_pmm.init(g_highest_address, bitmap_ptr);

    Multiboot::parseMemoryMap(multiboot_info_ptr, freeAvailableRegions);

    uint64_t bitmap_size = PhysicalMemoryManager::bitmapSizeFor(g_highest_address);
    g_pmm.markRegionUsed(0x0, bitmap_addr + bitmap_size);

    VMM::init();
    printf("%rVMM initialized.%r\n", VGA::GREEN, VGA::WHITE);

    Heap::init();
    printf("%rHeap initialized.%r\n", VGA::GREEN, VGA::WHITE);

    PCI::enumerate();
    printf("%rPCI initialized, found %d devices.%r\n", VGA::GREEN, PCI::get_index(), VGA::WHITE);


    Shell::init();

    printf("\n%r", VGA::WHITE);
    Scheduler::init();
    g_terminal.setColor(0xF, 0x0);
    Scheduler::createTask(Shell::run);
    Scheduler::createTask(StatusBar::time);
    Scheduler::createTask(StatusBar::date);
    Scheduler::start();
}