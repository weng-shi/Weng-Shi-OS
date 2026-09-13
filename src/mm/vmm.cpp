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

#include <vmm.h>
#include <pmm.h>
#include <terminal.h>

extern "C" uint64_t p4_table_phys_addr;

namespace VMM {
    namespace {
        constexpr uint64_t RECURSIVE_INDEX = 510;
        constexpr uint64_t ADDR_MASK = 0x000FFFFFFFFFF000ull;
        constexpr uint64_t PAGE_SIZE_BIT = 1ull << 7;
        constexpr uint64_t PAGE_PWT = 1ull << 3;
        constexpr uint64_t HUGE_2MB = 512 * PAGE_SIZE;
        constexpr uint64_t HUGE_1GB = 512 * HUGE_2MB;

        constexpr uint64_t KERNEL_PLM4_INDEX = 511;

        uint64_t canonicalize(uint64_t addr) {
            if (addr & (1ull << 47)) addr |= 0xFFFF000000000000ull;
            return addr;
        }

        uint64_t *pml4Virt() {
            uint64_t addr = (RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) |
                            (RECURSIVE_INDEX << 21) | (RECURSIVE_INDEX << 12);
            return reinterpret_cast<uint64_t *>(canonicalize(addr));
        }

        uint64_t *pdptVirt(uint64_t pml4_idx) {
            uint64_t addr = (RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) |
                            (RECURSIVE_INDEX << 21) | (pml4_idx << 12);
            return reinterpret_cast<uint64_t *>(canonicalize(addr));
        }

        uint64_t *pdVirt(uint64_t pml4_idx, uint64_t pdpt_idx) {
            uint64_t addr = (RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) |
                            (pml4_idx << 21) | (pdpt_idx << 12);
            return reinterpret_cast<uint64_t *>(canonicalize(addr));
        }

        uint64_t *ptVirt(uint64_t pml4_idx, uint64_t pdpt_idx, uint64_t pd_idx) {
            uint64_t addr = (RECURSIVE_INDEX << 39) | (pml4_idx << 30) |
                            (pdpt_idx << 21) | (pd_idx << 12);
            return reinterpret_cast<uint64_t *>(canonicalize(addr));
        }

        void invlpg(uint64_t addr) {
            asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
        }

        void zeroPage(uint64_t *virt) {
            for (int i = 0; i < 512; ++i) virt[i] = 0;
        }

        void splitHugePage(uint64_t *parent_virt, uint64_t index,
                           uint64_t *child_virt_addr_for_zeroing,
                           uint64_t huge_page_size) {
            uint64_t huge_entry = parent_virt[index];
            uint64_t huge_base = huge_entry & ADDR_MASK;
            uint64_t flags = huge_entry &
                             (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_PWT | PAGE_CACHE_DISABLE);

            uint64_t new_frame = g_pmm.allocFrame();
            parent_virt[index] = new_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
            invlpg(reinterpret_cast<uint64_t>(child_virt_addr_for_zeroing));
            zeroPage(child_virt_addr_for_zeroing);

            uint64_t *child = child_virt_addr_for_zeroing;
            for (uint64_t i = 0; i < huge_page_size / PAGE_SIZE; ++i) {
                child[i] = (huge_base + i * PAGE_SIZE) | flags;
            }

            invlpg(huge_base);
        }

        uint64_t ensureTable(uint64_t *parent_virt, uint64_t index, uint64_t *child_virt_addr_for_zeroing,
                             uint64_t huge_page_size) {
            if (parent_virt[index] & PAGE_PRESENT) {
                if (parent_virt[index] & PAGE_SIZE_BIT) {
                    if (huge_page_size == 0) {
                        g_terminal.write("VMM FATAL: attempted to walk through a huge page as if it were a table!\n");
                        for (;;) asm volatile("hlt");
                    }
                    splitHugePage(parent_virt, index, child_virt_addr_for_zeroing, huge_page_size);
                }
            } else {
                uint64_t new_frame = g_pmm.allocFrame();
                parent_virt[index] = new_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
                invlpg(reinterpret_cast<uint64_t>(child_virt_addr_for_zeroing));
                zeroPage(child_virt_addr_for_zeroing);
            }
            return parent_virt[index] & ADDR_MASK;
        }
    }

    void init() {
    }

    void mapPage(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags) {
        virtual_addr &= ~(PAGE_SIZE - 1);
        physical_addr &= ~(PAGE_SIZE - 1);

        uint64_t idx4 = (virtual_addr >> 39) & 0x1FF;
        uint64_t idx3 = (virtual_addr >> 30) & 0x1FF;
        uint64_t idx2 = (virtual_addr >> 21) & 0x1FF;
        uint64_t idx1 = (virtual_addr >> 12) & 0x1FF;

        uint64_t *pml4 = pml4Virt();
        ensureTable(pml4, idx4, pdptVirt(idx4), 0);

        uint64_t *pdpt = pdptVirt(idx4);
        ensureTable(pdpt, idx3, pdVirt(idx4, idx3), HUGE_1GB);

        uint64_t *pd = pdVirt(idx4, idx3);
        ensureTable(pd, idx2, ptVirt(idx4, idx3, idx2), HUGE_2MB);

        uint64_t *pt = ptVirt(idx4, idx3, idx2);
        pt[idx1] = physical_addr | flags | PAGE_PRESENT;

        invlpg(virtual_addr);
    }

    void unmapPage(uint64_t virtual_addr) {
        virtual_addr &= ~(PAGE_SIZE - 1);

        uint64_t idx4 = (virtual_addr >> 39) & 0x1FF;
        uint64_t idx3 = (virtual_addr >> 30) & 0x1FF;
        uint64_t idx2 = (virtual_addr >> 21) & 0x1FF;
        uint64_t idx1 = (virtual_addr >> 12) & 0x1FF;

        uint64_t *pml4 = pml4Virt();
        if (!(pml4[idx4] & PAGE_PRESENT)) return;

        uint64_t *pdpt = pdptVirt(idx4);
        if (!(pdpt[idx3] & PAGE_PRESENT)) return;

        uint64_t *pd = pdVirt(idx4, idx3);
        if (!(pd[idx2] & PAGE_PRESENT)) return;

        uint64_t *pt = ptVirt(idx4, idx3, idx2);
        pt[idx1] = 0;

        invlpg(virtual_addr);
    }

    bool allocAndMapPage(uint64_t virtual_addr, uint64_t flags) {
        uint64_t frame = g_pmm.allocFrame();
        if (frame == UINT64_MAX) return false;
        mapPage(virtual_addr, frame, flags);
        return true;
    }

    uint64_t currentAddressSpace() {
        uint64_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        return cr3 & ADDR_MASK;
    }

    void switchAddressSpace(uint64_t plm4_phys) {
        asm volatile("mov %0, %%cr3" :: "r"(plm4_phys) : "memory");
    }

    uint64_t createAddressSpace() {
        uint64_t new_plm4_phys = g_pmm.allocFrame();
        if (new_plm4_phys == UINT64_MAX) return 0;

        constexpr uint64_t SCRATCH_VIRT = 0xFFFFFFFFC6000000ull;
        mapPage(SCRATCH_VIRT, new_plm4_phys, PAGE_WRITABLE);

        auto *new_plm4 = reinterpret_cast<uint64_t *>(SCRATCH_VIRT);
        for (int i = 0; i < 512; ++i) new_plm4[i] = 0;

        uint64_t *current_plm4 = pml4Virt();

		new_plm4[0] = current_plm4[0];

		new_plm4[RECURSIVE_INDEX] = new_plm4_phys | PAGE_PRESENT | PAGE_WRITABLE;
		new_plm4[KERNEL_PLM4_INDEX] = current_plm4[KERNEL_PLM4_INDEX];

		unmapPage(SCRATCH_VIRT);

		return new_plm4_phys;
    }
}
