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

namespace AHCI {
    struct HBAPort {
        uint32_t clb;
        uint32_t clbu;
        uint32_t fb;
        uint32_t fbu;
        uint32_t is;
        uint32_t ie;
        uint32_t cmd;
        uint32_t reserved0;
        uint32_t tfd;
        uint32_t sig;
        uint32_t ssts;
        uint32_t sctl;
        uint32_t serr;
        uint32_t sact;
        uint32_t ci;
        uint32_t sntf;
        uint32_t fbs;
        uint8_t reserved1[0x70 - 0x44];
        uint8_t vendor[0x80 - 0x70];
    } __attribute__((packed));

    struct HBAMemory {
        uint32_t cap;
        uint32_t ghc;
        uint32_t is;
        uint32_t pi;
        uint32_t vs;
        uint32_t ccc_ctl;
        uint32_t ccc_ptl;
        uint32_t em_loc;
        uint32_t em_ctl;
        uint32_t cap2;
        uint32_t bohc;
        uint8_t reserved[0xA0 - 0x2C];
        uint8_t vendor[0x100 - 0xA0];
        HBAPort ports[32];
    } __attribute__((packed));

    enum class DeviceType {
        None,
        SATA,
        SATAPI,
        SEMB,
        PortMultiplier,
        Unknown
    };

    struct HBACommandHeader {
        uint8_t cfl : 5;
        uint8_t a : 1;
        uint8_t w : 1;
        uint8_t p : 1;
        uint8_t r : 1;
        uint8_t b : 1;
        uint8_t c : 1;
        uint8_t reserved0 : 1;
        uint8_t pmp : 4;
        uint16_t prdtl;
        volatile uint32_t prdbc;
        uint32_t ctba;
        uint32_t ctbau;
        uint32_t reserved1[4];
    } __attribute__((packed));

    struct PRDTEntry {
        uint32_t dba;
        uint32_t dbau;
        uint32_t reserved0;
        uint32_t dbc : 22;
        uint32_t reserved1 : 9;
        uint32_t i : 1;
    } __attribute__((packed));

    struct HBACommandTable {
        uint8_t cfis[64];
        uint8_t acmd[16];
        uint8_t reserved[48];
        PRDTEntry prdt_entry[1];
    } __attribute__((packed));

    struct FisRegH2D {
        uint8_t  fis_type;
        uint8_t  pmport : 4;
        uint8_t  reserved0 : 3;
        uint8_t  c : 1;
        uint8_t  command;
        uint8_t  featurel;

        uint8_t  lba0;
        uint8_t  lba1;
        uint8_t  lba2;
        uint8_t  device;

        uint8_t  lba3;
        uint8_t  lba4;
        uint8_t  lba5;
        uint8_t  featureh;

        uint8_t  countl;
        uint8_t  counth;
        uint8_t  icc;
        uint8_t  control;

        uint32_t reserved1;
    } __attribute__((packed));

    constexpr int MAX_PORTS = 32;

    bool init();
    bool isInitialized();

    bool identify(int port_index, uint16_t* out_buffer_512_words);

    DeviceType portDeviceType(int port_index);
    bool portHasDevice(int port_index);

    bool readSectors(int port_index, uint64_t lba, uint32_t sector_count, void *out_buffer);
    bool writeSectors(int port_index, uint64_t lba, uint32_t sector_count, const void *in_buffer);



    HBAMemory *hba();
}
