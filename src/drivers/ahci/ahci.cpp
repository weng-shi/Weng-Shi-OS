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

#include <ahci.h>
#include <pci.h>
#include <vmm.h>
#include <terminal.h>
#include <serial.h>
#include <io.h>
#include <pmm.h>
#include <vfs.h>
#include <timer.h>
#include <printf.h>

namespace AHCI {
    namespace {
        HBAMemory *g_hba = nullptr;

        bool g_initialized = false;

        constexpr uint8_t PCI_CLASS_MASS_STORAGE = 0x01;
        constexpr uint8_t PCI_SUBCLASS_SATA = 0x06;
        constexpr uint64_t ABAR_VIRT_ADDR = 0xFFFFFFFFC1000000ull;
        constexpr uint64_t PORT_BUFFER_VIRT_BASE = 0xFFFFFFFFC2000000ull;

        constexpr uint32_t SSTS_DET_MASK = 0x0F;
        constexpr uint32_t SSTS_DET_PRESENT = 0x03;
        constexpr uint32_t SSTS_IPM_SHIFT = 0x08;
        constexpr uint32_t SSTS_IPM_MASK = 0x0F;
        constexpr uint32_t SSTS_IPM_ACTIVE = 0x01;

        constexpr uint32_t SIG_ATA = 0x00000101;
        constexpr uint32_t SIG_ATAPI = 0xEB140101;
        constexpr uint32_t SIG_SEMB = 0xC33C0101;
        constexpr uint32_t SIG_PM = 0x96690101;

        constexpr uint32_t CMD_ST = 1u << 0;
        constexpr uint32_t CMD_SUD = 1u << 1;
        constexpr uint32_t CMD_POD = 1u << 2;
        constexpr uint32_t CMD_FRE = 1u << 4;
        constexpr uint32_t CMD_CR = 1u << 15;

        constexpr uint64_t CMDTABLE_VIRT_BASE = 0xFFFFFFFFC3000000ull;
        constexpr uint64_t IDENTIFY_BUF_VIRT_BASE = 0xFFFFFFFFC4000000ull;

        constexpr uint8_t ATA_CMD_IDENTIFY = 0xEC;
        constexpr uint8_t FIS_TYPE_REG_H2D = 0x27;

        constexpr uint32_t TFD_STS_BSY = 1u << 7;
        constexpr uint32_t TFD_STS_DRQ = 1u << 3;

        constexpr uint8_t ATA_CMD_READ_DMA_EXT = 0x25;
        constexpr uint8_t ATA_CMD_WRITE_DMA_EXT = 0x35;

        constexpr uint64_t IO_BUF_VIRT_BASE = 0xFFFFFFFFC5000000ull;
        constexpr uint32_t MAX_SECTORS_PER_TRANSFER = 8;

        uint64_t g_cmdtable_phys[MAX_PORTS] = {0};

        void setupPortBuffers(HBAPort &port, int port_idx) {
            uint64_t phys = g_pmm.allocFrame();
            if (phys == UINT64_MAX) return;

            uint64_t virt = PORT_BUFFER_VIRT_BASE + port_idx * VMM::PAGE_SIZE;
            VMM::mapPage(virt, phys, VMM::PAGE_WRITABLE | VMM::PAGE_CACHE_DISABLE);

            volatile uint64_t *p = reinterpret_cast<volatile uint64_t *>(virt);
            for (int i = 0; i < 512; ++i) p[i] = 0;

            port.clb = phys & 0xFFFFFFFFu;
            port.clbu = (phys >> 32) & 0xFFFFFFFFu;
            uint64_t fis = phys + 0x400;
            port.fb = fis & 0xFFFFFFFFu;
            port.fbu = (fis >> 32) & 0xFFFFFFFFu;

            uint64_t ct_phys = g_pmm.allocFrame();
            if (ct_phys == UINT64_MAX) return;

            uint64_t ct_virt = CMDTABLE_VIRT_BASE + port_idx * VMM::PAGE_SIZE;
            VMM::mapPage(ct_virt, ct_phys, VMM::PAGE_WRITABLE | VMM::PAGE_CACHE_DISABLE);

            volatile uint64_t *ctp = reinterpret_cast<volatile uint64_t *>(ct_virt);
            for (int i = 0; i < 512; ++i) ctp[i] = 0;

            g_cmdtable_phys[port_idx] = ct_phys;

            auto *cmd_headers = reinterpret_cast<volatile HBACommandHeader *>(virt);
            cmd_headers[0].ctba = ct_phys & 0xFFFFFFFFu;
            cmd_headers[0].ctbau = (ct_phys >> 32) & 0xFFFFFFFFu;
            cmd_headers[0].prdtl = 1;
        }

        constexpr uint32_t GHC_HBA_RESET = 1u << 0;
        constexpr uint32_t GHC_AHCI_ENABLE = 1u << 31;

        static void delay_ms(uint32_t ms) {
            uint64_t start = Timer::getTicks();
            uint64_t ticks_to_wait = (ms * 100) / 1000;
            if (ticks_to_wait == 0) ticks_to_wait = 1;

            while (Timer::getTicks() - start < ticks_to_wait) {
                asm volatile("hlt");
            }
        }

        bool resetHBA(HBAMemory *hba) {
            Serial::write("AHCI: performing full HBA reset...\n");

            hba->ghc |= GHC_AHCI_ENABLE;

            hba->ghc |= GHC_HBA_RESET;

            bool cleared = false;
            for (int i = 0; i < 1000; ++i) {
                if (!(hba->ghc & GHC_HBA_RESET)) {
                    cleared = true;
                    break;
                }
                delay_ms(1);
            }

            hba->ghc |= GHC_AHCI_ENABLE;

            Serial::write(cleared ? "AHCI: HBA reset complete.\n" : "AHCI: HBA reset TIMEOUT!\n");
            return cleared;
        }


        bool initPort(HBAPort &port, int port_idx) {
            if ((port.ssts & SSTS_DET_MASK) == SSTS_DET_PRESENT) {
                setupPortBuffers(port, port_idx);
                port.cmd |= CMD_FRE;
                delay_ms(10);
                port.cmd |= CMD_ST;
                return true;
            }

            port.cmd &= ~(CMD_ST | CMD_FRE);
            for (int i = 0; i < 500; ++i) {
                if (!(port.cmd & CMD_CR)) break;
                delay_ms(1);
            }

            port.cmd |= (CMD_SUD | CMD_POD);
            delay_ms(10);

            uint32_t sctl = port.sctl;
            sctl = (sctl & ~0xFu) | 0x1u;
            port.sctl = sctl;
            delay_ms(1);
            sctl = port.sctl;
            sctl &= ~0xFu;
            port.sctl = sctl;

            bool detected = false;
            for (int i = 0; i < 100; ++i) {
                if ((port.ssts & SSTS_DET_MASK) == SSTS_DET_PRESENT) {
                    detected = true;
                    break;
                }
                delay_ms(1);
            }

            if (!detected) {
                port.serr = port.serr;
                return false;
            }

            setupPortBuffers(port, port_idx);
            port.cmd |= CMD_FRE;

            sctl = port.sctl;
            sctl = (sctl & ~0xFu) | 0x1u;
            port.sctl = sctl;
            delay_ms(1);
            sctl = port.sctl;
            sctl &= ~0xFu;
            port.sctl = sctl;

            for (int i = 0; i < 100; ++i) {
                if ((port.ssts & SSTS_DET_MASK) == SSTS_DET_PRESENT) break;
                delay_ms(1);
            }

            delay_ms(10);
            port.cmd |= CMD_ST;
            return true;
        }

        DeviceType classifyPort(HBAPort &port) {
            uint32_t ssts = port.ssts;
            uint8_t det = ssts & SSTS_DET_MASK;
            uint8_t ipm = (ssts >> SSTS_IPM_SHIFT) & SSTS_IPM_MASK;

            if (det != SSTS_DET_PRESENT) return DeviceType::None;
            if (ipm != SSTS_IPM_ACTIVE) return DeviceType::None;

            switch (port.sig) {
                case SIG_ATA: return DeviceType::SATA;
                case SIG_ATAPI: return DeviceType::SATAPI;
                case SIG_SEMB: return DeviceType::SEMB;
                case SIG_PM: return DeviceType::PortMultiplier;
                default: return DeviceType::Unknown;
            }
        }

        const char *deviceTypeName(DeviceType type) {
            switch (type) {
                case DeviceType::SATA: return "SATA Drive";
                case DeviceType::SATAPI: return "SATAPI (CD/DVD)";
                case DeviceType::SEMB: return "Enclosure Management Bridge";
                case DeviceType::PortMultiplier: return "Port Multiplier";
                case DeviceType::Unknown: return "Unknown Device";
                case DeviceType::None: return "None";
            }
            return "None";
        }

        DeviceType g_port_types[MAX_PORTS];

        bool issueCommand(HBAPort &port, int port_index, uint8_t ata_command,
                          uint64_t lba, uint16_t sector_count,
                          uint64_t buf_phys, uint32_t buf_bytes, bool is_write) {
            for (int i = 0; i < 1000; ++i) {
                if (!(port.tfd & (TFD_STS_BSY | TFD_STS_DRQ))) break;
                delay_ms(1);
            }

            uint64_t cmd_list_virt = PORT_BUFFER_VIRT_BASE + port_index * VMM::PAGE_SIZE;
            uint64_t ct_virt = CMDTABLE_VIRT_BASE + port_index * VMM::PAGE_SIZE;

            auto *cmd_header = reinterpret_cast<volatile HBACommandHeader *>(cmd_list_virt);
            auto *cmd_table = reinterpret_cast<volatile HBACommandTable *>(ct_virt);

            for (size_t i = 0; i < sizeof(HBACommandTable) / sizeof(uint32_t); ++i) {
                reinterpret_cast<volatile uint32_t *>(cmd_table)[i] = 0;
            }

            cmd_header[0].cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
            cmd_header[0].w = is_write ? 1 : 0;
            cmd_header[0].prdtl = 1;
            cmd_header[0].prdbc = 0;

            cmd_table->prdt_entry[0].dba = buf_phys & 0xFFFFFFFFu;
            cmd_table->prdt_entry[0].dbau = (buf_phys >> 32) & 0xFFFFFFFFu;
            cmd_table->prdt_entry[0].dbc = buf_bytes - 1;
            cmd_table->prdt_entry[0].i = 1;

            auto *fis = reinterpret_cast<volatile FisRegH2D *>(cmd_table->cfis);
            fis->fis_type = FIS_TYPE_REG_H2D;
            fis->c = 1;
            fis->command = ata_command;
            fis->featurel = 0;
            fis->featureh = 0;

            fis->lba0 = static_cast<uint8_t>(lba & 0xFF);
            fis->lba1 = static_cast<uint8_t>((lba >> 8) & 0xFF);
            fis->lba2 = static_cast<uint8_t>((lba >> 16) & 0xFF);
            fis->lba3 = static_cast<uint8_t>((lba >> 24) & 0xFF);
            fis->lba4 = static_cast<uint8_t>((lba >> 32) & 0xFF);
            fis->lba5 = static_cast<uint8_t>((lba >> 40) & 0xFF);

            fis->device = 0x40;

            fis->countl = static_cast<uint8_t>(sector_count & 0xFF);
            fis->counth = static_cast<uint8_t>((sector_count >> 8) & 0xFF);

            port.ci |= 1u;

            bool success = false;

            for (int i = 0; i < 10000; ++i) {
                if (!(port.ci & 1u)) {
                    success = true;
                    break;
                }
                if (port.is & (1u << 30)) break;
                delay_ms(1);
            }

            if (!success) {
                Serial::write("AHCI: command timed out on port ");
                Serial::writeDec(port_index);
                Serial::write("\n");
                return false;
            }

            if (port.tfd & 0x1) {
                Serial::write("AHCI: command reported error on port ");
                Serial::writeDec(port_index);
                Serial::write("\n");
                return false;
            }

            return true;
        }
    }

    bool isInitialized() {
        return g_initialized;
    }

    HBAMemory *hba() {
        return g_hba;
    }

    DeviceType portDeviceType(int port_index) {
        if (port_index < 0 || port_index >= MAX_PORTS) return DeviceType::None;

        return g_port_types[port_index];
    }

    bool portHasDevice(int port_index) {
        return portDeviceType(port_index) != DeviceType::None;
    }

    bool init() {
        uint16_t device_count = PCI::get_index();
        bool found = false;
        uint32_t abar_phys = 0;
        uint8_t ahci_bus = 0, ahci_slot = 0, ahci_func = 0;

        for (uint16_t i = 0; i < device_count; ++i) {
            const PCI::PCIDevice &dev = PCI::getDevice(i);

            if (dev.header.base_class == PCI_CLASS_MASS_STORAGE &&
                dev.header.sub_class == PCI_SUBCLASS_SATA) {
                abar_phys = dev.header.BAR5;
                ahci_bus = dev.bus;
                ahci_slot = dev.slot;
                ahci_func = dev.func;
                found = true;
                break;
            }
        }

        if (!found) {
                printf("%rAHCI: No Controller Found!%r\n", VGA::RED, VGA::WHITE);
            return false;
        }

        uint32_t pci_cmd_addr = PCI::getAddress(ahci_bus, ahci_slot, ahci_func, 0x4);
        uint32_t pci_cmd = PCI::configReadDword(pci_cmd_addr);
        pci_cmd |= (1u << 2);
        PCI::configWriteDword(pci_cmd_addr, pci_cmd);

        abar_phys &= 0xFFFFFFF0;

        if (abar_phys == 0) {
            printf("%rAHCI: BAR5 Is Invalid!%r\n", VGA::RED, VGA::WHITE);
            return false;
        }

        for (int page = 0; page < 2; ++page) {
            uint64_t phys = abar_phys + page * VMM::PAGE_SIZE;
            uint64_t virt = ABAR_VIRT_ADDR + page * VMM::PAGE_SIZE;
            VMM::mapPage(virt, phys, VMM::PAGE_WRITABLE | VMM::PAGE_CACHE_DISABLE);
        }

        g_hba = reinterpret_cast<HBAMemory *>(ABAR_VIRT_ADDR);
        g_initialized = true;

        resetHBA(g_hba);

        printf("%rAHCI: Controller initialized. Ports implemented bitmap: %r%x%r\n", VGA::GREEN, VGA::YELLOW, g_hba->pi, VGA::WHITE);

        if (!(g_hba->ghc & GHC_AHCI_ENABLE)) {
            g_hba->ghc |= GHC_AHCI_ENABLE;
        }

        printf("%rAHCI: Scanning ports%r\n", VGA::YELLOW, VGA::WHITE);
        int found_devices = 0;

        int max_check = (g_hba->cap & 0x1F) + 1;
        if (max_check > MAX_PORTS) max_check = MAX_PORTS;

        for (int i = 0; i < max_check; ++i) {
            if (!(g_hba->pi & (1u << i))) {
                g_port_types[i] = DeviceType::None;
                continue;
            }

            if (!initPort(g_hba->ports[i], i)) {
                g_port_types[i] = DeviceType::None;
                continue;
            }

            DeviceType type = classifyPort(g_hba->ports[i]);
            g_port_types[i] = type;
            if (type == DeviceType::SATA) {
                uint16_t identify_buf[256];
                uint64_t sector_count = 0;

                if (identify(i, identify_buf)) {
                    sector_count = static_cast<uint32_t>(identify_buf[60]) |
                                   (static_cast<uint32_t>(identify_buf[61]) << 16);
                }

                char name[16];
                name[0] = 's'; name[1] = 'a'; name[2] = 't'; name[3] = 'a';
                name[4] = '0' + i;
                name[5] = '\0';

                VFS::registerBlockDevice(name, i, sector_count, 512,
                    [](int handle, uint64_t lba, uint32_t count, void* buf) {
                        return readSectors(handle, lba, count, buf);
                    },
                    [](int handle, uint64_t lba, uint32_t count, const void* buf) {
                        return writeSectors(handle, lba, count, buf);
                    });
            }

            if (type != DeviceType::None) {
                printf("%r  Port %d: %s%r\n", VGA::YELLOW, i, deviceTypeName(type), VGA::WHITE);

                ++found_devices;
            }
        }

        for (int i = 0; i < max_check; ++i) {
            if (!(g_hba->pi & (1u << i))) continue;
            Serial::write("AHCI: port ");
            Serial::writeDec(i);
            Serial::write(" ssts=");
            Serial::writeHex(g_hba->ports[i].ssts);
            Serial::write(" sig=");
            Serial::writeHex(g_hba->ports[i].sig);
            Serial::write(" type=");
            Serial::write(deviceTypeName(g_port_types[i]));
            Serial::write("\n");
        }

        if (found_devices) {
            printf("%rAHCI: Found %d device(s)%r\n", VGA::YELLOW, found_devices, VGA::WHITE);
        }

        return true;
    }

    bool identify(int port_index, uint16_t *out_buffer_512_words) {
        if (port_index < 0 || port_index >= MAX_PORTS) return false;
        if (!g_initialized || !portHasDevice(port_index)) return false;

        HBAPort &port = g_hba->ports[port_index];

        uint64_t buf_phys = g_pmm.allocFrame();
        if (buf_phys == UINT64_MAX) return false;

        uint64_t buf_virt = IDENTIFY_BUF_VIRT_BASE + port_index * VMM::PAGE_SIZE;
        VMM::mapPage(buf_virt, buf_phys, VMM::PAGE_WRITABLE | VMM::PAGE_CACHE_DISABLE);

        for (int i = 0; i < 1000; ++i) {
            if (!(port.tfd & (TFD_STS_BSY | TFD_STS_DRQ))) break;
            delay_ms(1);
        }

        uint64_t cmd_list_virt = PORT_BUFFER_VIRT_BASE + port_index * VMM::PAGE_SIZE;
        uint64_t ct_virt = CMDTABLE_VIRT_BASE + port_index * VMM::PAGE_SIZE;

        auto *cmd_header = reinterpret_cast<volatile HBACommandHeader *>(cmd_list_virt);
        auto *cmd_table = reinterpret_cast<volatile HBACommandTable *>(ct_virt);

        for (size_t i = 0; i < sizeof(HBACommandTable) / sizeof(uint32_t); ++i) {
            reinterpret_cast<volatile uint32_t *>(cmd_table)[i] = 0;
        }

        cmd_header[0].cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
        cmd_header[0].w = 0;
        cmd_header[0].prdtl = 1;
        cmd_header[0].prdbc = 0;

        cmd_table->prdt_entry[0].dba = buf_phys & 0xFFFFFFFFu;
        cmd_table->prdt_entry[0].dbau = (buf_phys >> 32) & 0xFFFFFFFFu;
        cmd_table->prdt_entry[0].dbc = 511;
        cmd_table->prdt_entry[0].i = 1;

        auto *fis = reinterpret_cast<volatile FisRegH2D *>(cmd_table->cfis);
        fis->fis_type = FIS_TYPE_REG_H2D;
        fis->c = 1;
        fis->command = ATA_CMD_IDENTIFY;
        fis->device = 0;
        fis->featurel = 0;
        fis->featureh = 0;
        fis->lba0 = fis->lba1 = fis->lba2 = 0;
        fis->lba3 = fis->lba4 = fis->lba5 = 0;
        fis->countl = 0;
        fis->counth = 0;

        port.ci |= 1u;

        bool success = false;
        for (int i = 0; i < 1000; ++i) {
            if (!(port.ci & 1u)) {
                success = true;
                break;
            }
            if (port.is & (1u << 30)) break;
            delay_ms(1);
        }

        if (!success) {
            Serial::write("AHCI: IDENTIFY command timed out on port ");
            Serial::writeDec(port_index);
            Serial::write("\n");
            return false;
        }

        if (port.tfd & 0x1) {
            Serial::write("AHCI: IDENTIFY command reported error on port ");
            Serial::writeDec(port_index);
            Serial::write("\n");
            return false;
        }

        volatile uint16_t *src = reinterpret_cast<volatile uint16_t *>(buf_virt);
        for (int i = 0; i < 256; ++i) {
            out_buffer_512_words[i] = src[i];
        }

        return true;
    }

    bool readSectors(int port_index, uint64_t lba, uint32_t sector_count, void *out_buffer) {
        if (port_index < 0 || port_index >= MAX_PORTS) return false;
        if (!g_initialized || !portHasDevice(port_index)) return false;

        if (sector_count == 0 || sector_count > MAX_SECTORS_PER_TRANSFER) return false;

        uint32_t bytes = sector_count * 512;

        uint64_t buf_phys = g_pmm.allocFrame();

        if (buf_phys == UINT64_MAX) return false;

        uint64_t buf_virt = IO_BUF_VIRT_BASE + port_index * VMM::PAGE_SIZE;

        VMM::mapPage(buf_virt, buf_phys, VMM::PAGE_WRITABLE | VMM::PAGE_CACHE_DISABLE);

        HBAPort &port = g_hba->ports[port_index];

        bool ok = issueCommand(port, port_index, ATA_CMD_READ_DMA_EXT, lba,
                                static_cast<uint16_t>(sector_count), buf_phys, bytes, false);

        if (ok) {
            auto *src = reinterpret_cast<volatile uint8_t *>(buf_virt);
            auto *dst = reinterpret_cast<uint8_t *>(out_buffer);
            for (uint32_t i = 0; i < bytes; ++i) dst[i] = src[i];
        }

        return ok;
    }

    bool writeSectors(int port_index, uint64_t lba, uint32_t sector_count, const void *in_buffer) {
        if (port_index < 0 || port_index >= MAX_PORTS) return false;
        if (!g_initialized || !portHasDevice(port_index)) return false;

        if (sector_count == 0 || sector_count > MAX_SECTORS_PER_TRANSFER) return false;

        uint32_t bytes = sector_count * 512;

        uint64_t buf_phys = g_pmm.allocFrame();

        if (buf_phys == UINT64_MAX) return false;

        uint64_t buf_virt = IO_BUF_VIRT_BASE + port_index * VMM::PAGE_SIZE;

        VMM::mapPage(buf_virt, buf_phys, VMM::PAGE_WRITABLE | VMM::PAGE_CACHE_DISABLE);

        auto *dst = reinterpret_cast<volatile uint8_t *>(buf_virt);
        auto *src = reinterpret_cast<const uint8_t *>(in_buffer);
        for (uint32_t i = 0; i < bytes; ++i) dst[i] = src[i];

        HBAPort &port = g_hba->ports[port_index];
        return issueCommand(port, port_index, ATA_CMD_WRITE_DMA_EXT, lba,
                            static_cast<uint16_t>(sector_count), buf_phys, bytes, true);
    }
}
