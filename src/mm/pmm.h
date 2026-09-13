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

#include  <stdint.h>
#include  <stddef.h>

class PhysicalMemoryManager {
public:
    static constexpr uint64_t FRAME_SIZE = 4096;

    void init(uint64_t highest_address, uint8_t *bitmap_storage);

    void markRegionFree(uint64_t base, uint64_t length);
    void markRegionUsed(uint64_t base, uint64_t length);

    uint64_t allocFrame();
    void freeFrame(uint64_t phys_address);

    uint64_t totalFrames() const { return total_frames_; }
    uint64_t usedFrames() const { return used_frames_; }
    uint64_t freeFrames () const { return total_frames_ - used_frames_; }

    static uint64_t bitmapSizeFor(uint64_t highest_address);

private:
    void setBit(uint64_t frame_index);
    void clearBit(uint64_t frame_index);
    bool testBit(uint64_t frame_index) const;

    uint8_t *bitmap_;
    uint64_t total_frames_;
    uint64_t used_frames_;
    uint64_t search_hint_;
};

extern PhysicalMemoryManager g_pmm;
