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

#include <pci.h>
#include <io.h>

namespace PCI {
    namespace {
        PCIDevice g_pci_devices[MAX_PCI_DEVICES];
        uint16_t g_pci_devices_index = 0;
    }

    uint16_t configReadWord(const uint32_t address, const uint8_t offset) {
        outl(0xCF8, address);
        return static_cast<uint16_t>((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
    }

    uint32_t configReadDword(const uint32_t address) {
        outl(0xCF8, address);
        return inl(0xCFC);
    }

    void configWriteDword(const uint32_t address, uint32_t value) {
        outl(0xCF8, address);
        outl(0xCFC, value);
    }

    uint32_t getAddress(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t offset) {
        const auto lbus = static_cast<uint32_t>(bus);
        const auto lslot = static_cast<uint32_t>(slot);
        const auto lfunc = static_cast<uint32_t>(func);
        return (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | 0x80000000u;
    }

    bool deviceExists(uint32_t address) {
        uint16_t vendor = configReadDword(address) & 0xFFFF;
        return vendor != 0xFFFF;
    }

    void saveDevice(const uint8_t bus, const uint8_t slot, const uint8_t func) {
        if (g_pci_devices_index >= MAX_PCI_DEVICES) return; // guard against overflow

        PCIDevice &dev = g_pci_devices[g_pci_devices_index];
        uint32_t address;
        uint32_t tmp;

        address = getAddress(bus, slot, func, 0x0);
        tmp = configReadDword(address);
        dev.header.vendor_id = tmp & 0xFFFF;
        dev.header.device_id = (tmp >> 16) & 0xFFFF;

        address = getAddress(bus, slot, func, 0x4);
        tmp = configReadDword(address);
        dev.header.command_register = tmp & 0xFFFF;
        dev.header.status_register = (tmp >> 16) & 0xFFFF;

        address = getAddress(bus, slot, func, 0x8);
        tmp = configReadDword(address);
        dev.header.revision_id = tmp & 0xFF;
        dev.header.prog_if = (tmp >> 8) & 0xFF;
        dev.header.sub_class = (tmp >> 16) & 0xFF;
        dev.header.base_class = (tmp >> 24) & 0xFF;

        address = getAddress(bus, slot, func, 0xC);
        tmp = configReadDword(address);
        dev.header.cache_line_size = tmp & 0xFF;
        dev.header.latency_timer = (tmp >> 8) & 0xFF;
        dev.header.header_type = (tmp >> 16) & 0xFF;
        dev.header.bist = (tmp >> 24) & 0xFF;

        dev.header.BAR0 = configReadDword(getAddress(bus, slot, func, 0x10));
        dev.header.BAR1 = configReadDword(getAddress(bus, slot, func, 0x14));
        dev.header.BAR2 = configReadDword(getAddress(bus, slot, func, 0x18));
        dev.header.BAR3 = configReadDword(getAddress(bus, slot, func, 0x1C));
        dev.header.BAR4 = configReadDword(getAddress(bus, slot, func, 0x20));
        dev.header.BAR5 = configReadDword(getAddress(bus, slot, func, 0x24));

        dev.header.cardbus_cis_pointer = configReadDword(getAddress(bus, slot, func, 0x28));

        address = getAddress(bus, slot, func, 0x2C);
        tmp = configReadDword(address);
        dev.header.subsystem_vendor_id = tmp & 0xFFFF;
        dev.header.subsystem_device_id = (tmp >> 16) & 0xFFFF;

        dev.header.ex_rba = configReadDword(getAddress(bus, slot, func, 0x30));

        address = getAddress(bus, slot, func, 0x34);
        tmp = configReadDword(address);
        dev.header.capabilities_pointer = tmp & 0xFF;

        address = getAddress(bus, slot, func, 0x3C);
        tmp = configReadDword(address);
        dev.header.interrupt_line = tmp & 0xFF;
        dev.header.interrupt_pin = (tmp >> 8) & 0xFF;
        dev.header.min_grant = (tmp >> 16) & 0xFF;
        dev.header.max_latency = (tmp >> 24) & 0xFF;

        dev.bus = bus;
        dev.slot = slot;
        dev.func = func;

        ++g_pci_devices_index;
    }

    void enumerate() {
        g_pci_devices_index = 0;

        for (uint16_t bus = 0; bus < BUSSES; ++bus) {
            for (uint8_t slot = 0; slot < SLOTS; ++slot) {
                uint32_t base_address = getAddress(static_cast<uint8_t>(bus), slot, 0, 0);
                if (!deviceExists(base_address)) continue;

                saveDevice(static_cast<uint8_t>(bus), slot, 0);

                uint32_t header_type_addr = getAddress(static_cast<uint8_t>(bus), slot, 0, 0xC);
                uint8_t header_type = static_cast<uint8_t>((configReadDword(header_type_addr) >> 16) & 0xFF);

                if (header_type & 0x80) {
                    for (uint8_t func = 1; func < FUNCS; ++func) {
                        uint32_t addr = getAddress(static_cast<uint8_t>(bus), slot, func, 0);
                        if (deviceExists(addr)) {
                            saveDevice(static_cast<uint8_t>(bus), slot, func);
                        }
                    }
                }
            }
        }
    }

    uint16_t get_index() {
        return g_pci_devices_index;
    }

    const PCIDevice &getDevice(uint16_t index) {
        return g_pci_devices[index];
    }

    const char *classCodeToString(uint8_t base_class, uint8_t sub_class) {
        switch (base_class) {
            case 0x00: return "Unclassified";
            case 0x01: switch (sub_class) {
                    case 0x0: return "SCSI Bus Controller";
                    case 0x1: return "IDE Controller";
                    case 0x2: return "Floppy Disk Controller";
                    case 0x3: return "IPI Bus Controller";
                    case 0x4: return "Raid Controller";
                    case 0x5: return "ATA Controller";
                    case 0x6: return "Serial ATA Controller";
                    case 0x7: return "Serial Attached SCSI Controller";
                    case 0x8: return "Non-volatile Memory Controller";
                    default: return "Unknown";
                }
            case 0x02: switch (sub_class) {
                    case 0x0: return "Ethernet Controller";
                    default: return "Other Network Controller";
                }
            case 0x03: switch (sub_class) {
                    case 0x0: return "VGA Compatible Controller";
                    default: return "Other Display Controller";
                }
            case 0x04: return "Multimedia Controller";
            case 0x05: return "Memory Controller";
            case 0x06: return "Bridge Device";
            case 0x07: return "Simple Communication Controller";
            case 0x08: return "Base System Peripheral";
            case 0x09: return "Input Device Controller";
            case 0x0C: return "Serial Bus Controller";
            default: return "Unknown";
        }
    }
}
