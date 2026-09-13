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

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace PCI {
    constexpr uint16_t MAX_PCI_DEVICES = 64;
    constexpr uint16_t BUSSES = 256;
    constexpr uint8_t SLOTS = 32;
    constexpr uint8_t FUNCS = 8;

    struct GeneralDeviceHeader {
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t command_register;
        uint16_t status_register;
        uint8_t revision_id;
        uint8_t prog_if;
        uint8_t sub_class;
        uint8_t base_class;
        uint8_t cache_line_size;
        uint8_t latency_timer;
        uint8_t header_type;
        uint8_t bist;
        uint32_t BAR0;
        uint32_t BAR1;
        uint32_t BAR2;
        uint32_t BAR3;
        uint32_t BAR4;
        uint32_t BAR5;
        uint32_t cardbus_cis_pointer;
        uint16_t subsystem_vendor_id;
        uint16_t subsystem_device_id;
        uint32_t ex_rba;
        uint8_t capabilities_pointer;
        uint8_t reserved1;
        uint16_t reserved2;
        uint32_t reserved3;
        uint8_t interrupt_line;
        uint8_t interrupt_pin;
        uint8_t min_grant;
        uint8_t max_latency;
    };

    struct PCIDevice {
        uint8_t bus;
        uint8_t slot;
        uint8_t func;
        GeneralDeviceHeader header;
    };

    uint16_t configReadWord(uint32_t address, uint8_t offset);

    uint32_t configReadDword(uint32_t address);

    void configWriteDword(uint32_t address, uint32_t value);

    uint32_t getAddress(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

    bool deviceExists(uint32_t address);

    void saveDevice(uint8_t bus, uint8_t slot, uint8_t func);

    void enumerate();

    uint16_t get_index();

    const PCIDevice &getDevice(uint16_t index);

    const char *classCodeToString(uint8_t base_class, uint8_t sub_class);
}
