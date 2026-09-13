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

#include <idt.h>
#include <gdt.h>
#include <panic.h>
#include <pic.h>
#include <scheduler.h>

namespace {
    struct IdtEntry {
        uint16_t offset_low;
        uint16_t selector;
        uint8_t ist;
        uint8_t type_attr;
        uint16_t offset_mid;
        uint32_t offset_high;
        uint32_t zero;
    } __attribute__((packed));

    struct IdtPointer {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    constexpr int IDT_ENTRIES = 256;
    IdtEntry g_idt[IDT_ENTRIES];
    IdtPointer g_idt_ptr;



    void set_gate(int vector, uint64_t handler, uint16_t selector, uint8_t type_attr) {
        g_idt[vector].offset_low = handler & 0xFFFF;
        g_idt[vector].offset_mid = (handler >> 16) & 0xFFFF;
        g_idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
        g_idt[vector].selector = selector;
        g_idt[vector].ist = 0;
        g_idt[vector].type_attr = type_attr;
        g_idt[vector].zero = 0;
    }

    IrqHandler g_irq_handlers[16] = {nullptr};
}

extern "C" uint64_t g_next_task_rsp = 0;

extern "C" {
void isr0();

void isr1();

void isr2();

void isr3();

void isr4();

void isr5();

void isr6();

void isr7();

void isr8();

void isr9();

void isr10();

void isr11();

void isr12();

void isr13();

void isr14();

void isr15();

void isr16();

void isr17();

void isr18();

void isr19();

void isr20();

void isr21();

void isr22();

void isr23();

void isr24();

void isr25();

void isr26();

void isr27();

void isr28();

void isr29();

void isr30();

void isr31();

void isr128();
}

extern "C" {
void irq0();

void irq1();

void irq2();

void irq3();

void irq4();

void irq5();

void irq6();

void irq7();

void irq8();

void irq9();

void irq10();

void irq11();

void irq12();

void irq13();

void irq14();

void irq15();
}

extern "C" void idt_flush(uint64_t idt_ptr_addr) {
    asm volatile("lidt (%0)" :: "r"(idt_ptr_addr));
}

void idt_register_irq_handler(uint8_t irq, IrqHandler handler) {
    if (irq < 16) g_irq_handlers[irq] = handler;
}

void idt_init() {
    g_idt_ptr.limit = sizeof(g_idt) - 1;
    g_idt_ptr.base = reinterpret_cast<uint64_t>(&g_idt);

    constexpr uint8_t INTERRUPT_GATE = 0x8E;

    void (*isr_stubs[32])() = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; ++i) {
        set_gate(i, reinterpret_cast<uint64_t>(isr_stubs[i]), GDT_KERNEL_CODE, INTERRUPT_GATE);
    }

    void (*irq_stubs[16])() = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };
    for (int i = 0; i < 16; ++i) {
        set_gate(32 + i, reinterpret_cast<uint64_t>(irq_stubs[i]), GDT_KERNEL_CODE, INTERRUPT_GATE);
    }

    constexpr uint8_t SYSCALL_GATE = 0xEF;
    set_gate(0x80, reinterpret_cast<uint64_t>(isr128), GDT_KERNEL_CODE, SYSCALL_GATE);

    idt_flush(reinterpret_cast<uint64_t>(&g_idt_ptr));
}

extern "C" void isr_handler(Registers *regs) {
    kernel_panic(regs);
}

extern "C" void irq_handler(Registers *regs) {
    g_next_task_rsp = reinterpret_cast<uint64_t>(regs);
    uint8_t irq = static_cast<uint8_t>(regs->vector);
    if (irq < 16 && g_irq_handlers[irq]) {
        g_irq_handlers[irq]();
    }

    if (irq == 0) {
        Scheduler::tick(regs);
    }
    PIC::sendEOI(irq);
}
