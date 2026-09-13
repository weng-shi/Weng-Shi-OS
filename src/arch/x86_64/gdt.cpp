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

#include <gdt.h>

namespace {

    struct GdtEntry {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t  base_mid;
        uint8_t  access;
        uint8_t  granularity;
        uint8_t  base_high;
    } __attribute__((packed));

    struct TssDescriptor {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t base_mid;
        uint8_t access;
        uint8_t granularity;
        uint8_t base_high;
        uint32_t base_upper;
        uint32_t reserved;
    } __attribute__((packed));

    struct GdtPointer {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    struct TSS {
        uint32_t reserved0;
        uint64_t rsp0;
        uint64_t rsp1;
        uint64_t rsp2;
        uint64_t reserved1;
        uint64_t ist[7];
        uint64_t reserved2;
        uint16_t reserved3;
        uint16_t iomap_base;
    } __attribute__((packed));

    constexpr int GDT_ENTRIES = 5;

    GdtEntry g_gdt[GDT_ENTRIES];
    TssDescriptor g_tss_descriptor;
    GdtPointer g_gdt_ptr;
    TSS g_tss;

    void set_gate(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
        g_gdt[index].base_low    = base & 0xFFFF;
        g_gdt[index].base_mid    = (base >> 16) & 0xFF;
        g_gdt[index].base_high   = (base >> 24) & 0xFF;
        g_gdt[index].limit_low   = limit & 0xFFFF;
        g_gdt[index].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
        g_gdt[index].access      = access;
    }

    void set_tss_descriptor(uint64_t base, uint32_t limit) {
        g_tss_descriptor.limit_low = limit & 0xFFFF;
        g_tss_descriptor.base_low = base & 0xFFFF;
        g_tss_descriptor.base_mid = (base >> 16) & 0xFF;
        g_tss_descriptor.access = 0x89;
        g_tss_descriptor.granularity = (limit >> 16) & 0xFF;
        g_tss_descriptor.base_high = (base >> 24) & 0xFF;
        g_tss_descriptor.base_upper = (base >> 32) & 0xFFFFFFFF;
        g_tss_descriptor.reserved = 0;
    }

}

extern "C" void gdt_flush(uint64_t gdt_ptr_addr);
extern "C" void tss_flush();

void gdt_set_kernel_stack(uint64_t rsp0) {
    g_tss.rsp0 = rsp0;
}

void gdt_init() {
    static uint8_t gdt_storage[GDT_ENTRIES * sizeof(GdtEntry) + sizeof(TssDescriptor)];

    set_gate(0, 0, 0, 0, 0);
    set_gate(1, 0, 0xFFFF, 0x9A, 0xA0);
    set_gate(2, 0, 0xFFFF, 0x92, 0x80);
    set_gate(3, 0, 0xFFFF, 0xFA, 0xA0);
    set_gate(4, 0, 0xFFFF, 0xF2, 0x80);

    for (int i = 0; i < GDT_ENTRIES; ++i) {
        reinterpret_cast<GdtEntry *>(gdt_storage)[i] = g_gdt[i];
    }

    for (size_t i = 0; i < sizeof(TSS); ++i) {
        reinterpret_cast<uint8_t *>(&g_tss)[i] = 0;
    }

    g_tss.iomap_base = sizeof(TSS);

    set_tss_descriptor(reinterpret_cast<uint64_t>(&g_tss), sizeof(TSS) - 1);

    auto *tss_desc_ptr = reinterpret_cast<TssDescriptor *>(gdt_storage + GDT_ENTRIES * sizeof(GdtEntry));
    *tss_desc_ptr = g_tss_descriptor;

    g_gdt_ptr.limit = sizeof(gdt_storage) - 1;
    g_gdt_ptr.base = reinterpret_cast<uint64_t>(gdt_storage);

    gdt_flush(reinterpret_cast<uint64_t>(&g_gdt_ptr));
    tss_flush();
}