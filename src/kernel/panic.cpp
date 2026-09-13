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

#include <panic.h>
#include <terminal.h>
#include <serial.h>

namespace {
    const char* exception_messages[32] = {
        "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
        "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
        "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
        "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
        "Coprocessor Fault", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
        "Virtualization Exception", "Control Protection Exception",
        "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "Reserved", "Hypervisor Injection", "VMM Communication", "Security Exception"
    };

    uint64_t readCR2() {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        return cr2;
    }

    void writeBoth(const char* str) {
        g_terminal.write(str);
        Serial::write(str);
    }

    void writeHexBoth(uint64_t value) {
        g_terminal.writeHex(value);
        Serial::writeHex(value);
    }
}

[[noreturn]] void kernel_panic(Registers* regs) {
    g_terminal.setColor(0x0F, 0x04);
    g_terminal.clear();

    writeBoth("*** KERNEL PANIC ***\n\n");

    const char* name = (regs->vector < 32) ? exception_messages[regs->vector] : "Unknown";
    writeBoth("Exception: ");
    writeBoth(name);
    writeBoth("\n");

    if (regs->vector == 14) {
        writeBoth("Faulting address (CR2): ");
        writeHexBoth(readCR2());
        writeBoth("\n");
        writeBoth("Reason: ");
        writeBoth((regs->err_code & 1) ? "protection violation" : "page not present");
        writeBoth(", ");
        writeBoth((regs->err_code & 2) ? "write" : "read");
        writeBoth(", ");
        writeBoth((regs->err_code & 4) ? "user mode" : "kernel mode");
        writeBoth("\n");
    }

    writeBoth("Vector:   "); writeHexBoth(regs->vector);   writeBoth("\n");
    writeBoth("Error:    "); writeHexBoth(regs->err_code); writeBoth("\n");
    writeBoth("RIP:      "); writeHexBoth(regs->rip);      writeBoth("\n");
    writeBoth("CS:       "); writeHexBoth(regs->cs);       writeBoth("\n");
    writeBoth("RFLAGS:   "); writeHexBoth(regs->rflags);   writeBoth("\n");
    writeBoth("RSP:      "); writeHexBoth(regs->rsp);      writeBoth("\n");
    writeBoth("RAX:      "); writeHexBoth(regs->rax);      writeBoth("\n");
    writeBoth("RBX:      "); writeHexBoth(regs->rbx);      writeBoth("\n");
    writeBoth("RCX:      "); writeHexBoth(regs->rcx);      writeBoth("\n");
    writeBoth("RDX:      "); writeHexBoth(regs->rdx);      writeBoth("\n");

    asm volatile("cli");
    for (;;) asm volatile("hlt");
}