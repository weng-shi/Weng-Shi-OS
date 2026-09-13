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
  Serial::write("CP1: serial init\n");
  Serial::write("=====WengShi OS=====\n");
  call_global_constructors();
  Serial::write("CP2: ctors done\n");
  (void)multiboot_info_ptr;

  printf("%r===================================WengShi OS===================================%r\n", VGA::BLUE, VGA::WHITE);

  g_terminal.enableHardwareCursor();
  Serial::write("CP3: vga cursor\n");
  Serial::write("VGA screen initialized\n");

  gdt_init();
  Serial::write("CP4: gdt done\n");
  printf("%rGDT initialized.%r\n", VGA::GREEN, VGA::WHITE);

  idt_init();
  Serial::write("CP5: idt done\n");
  printf("%rIDT initialized.%r\n", VGA::GREEN, VGA::WHITE);

  PIC::remap(0x20, 0x28);
  Serial::write("CP6: pic done\n");
  printf("%rPIC initialized.%r\n", VGA::GREEN, VGA::WHITE);

  Keyboard::init();
  idt_register_irq_handler(1, Keyboard::onIRQ);
  PIC::clearMask(1);
  Serial::write("CP7: keyboard done\n");
  printf("%rKeyboard initialized.%r\n", VGA::GREEN, VGA::WHITE);

  Timer::init(1000);
  PIC::clearMask(0);
  Serial::write("CP8: timer done\n");
  printf("%rPIT initialized.%r\n", VGA::GREEN, VGA::WHITE);

  asm volatile("sti");
  Serial::write("CP9: interrupts enabled\n");
  printf("%rInterrupts initialized.%r\n", VGA::GREEN, VGA::WHITE);

  Multiboot::parseMemoryMap(multiboot_info_ptr, findHighestAddress);
  Serial::write("CP10: highest_address = ");
  Serial::writeHex(g_highest_address);
  Serial::write("\n");

  uint64_t kernel_end_addr = reinterpret_cast<uint64_t>(kernel_physical_end);
  uint64_t bitmap_addr = (kernel_end_addr + 0xFFF) & ~0xFFFull;
  uint8_t* bitmap_ptr = reinterpret_cast<uint8_t*>(bitmap_addr);
  Serial::write("CP11: bitmap_addr = ");
  Serial::writeHex(bitmap_addr);
  Serial::write("\n");

  g_pmm.init(g_highest_address, bitmap_ptr);
  Serial::write("CP12: pmm init done\n");

  Multiboot::parseMemoryMap(multiboot_info_ptr, freeAvailableRegions);
  Serial::write("CP13: free regions done\n");

  uint64_t bitmap_size = PhysicalMemoryManager::bitmapSizeFor(g_highest_address);
  Serial::write("CP14: bitmap_size = ");
  Serial::writeHex(bitmap_size);
  Serial::write("\n");

  g_pmm.markRegionUsed(0x0, bitmap_addr + bitmap_size);
  Serial::write("CP15: mark used done\n");

  VMM::init();
  Serial::write("CP16: vmm init done\n");
  printf("%rVMM initialized.%r\n", VGA::GREEN, VGA::WHITE);

  Heap::init();
  Serial::write("CP17: heap init done\n");
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
