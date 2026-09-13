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

#include <pmm.h>

PhysicalMemoryManager g_pmm;

uint64_t PhysicalMemoryManager::bitmapSizeFor(uint64_t highest_address) {
    uint64_t frame_count = highest_address / FRAME_SIZE;
    return (frame_count + 7) / 8;
}

void PhysicalMemoryManager::init(uint64_t highest_address, uint8_t *bitmap_storage) {
    bitmap_ = bitmap_storage;
    total_frames_ = highest_address / FRAME_SIZE;
    used_frames_ = total_frames_;
    search_hint_ = 0;

    uint64_t bitmap_bytes = bitmapSizeFor(highest_address);
    for (uint64_t i = 0; i < bitmap_bytes; ++i) {
        bitmap_[i] = 0xFF;
    }
}

void PhysicalMemoryManager::setBit(uint64_t frame_index) {
    bitmap_[frame_index / 8] |= (1 << (frame_index % 8));
}

void PhysicalMemoryManager::clearBit(uint64_t frame_index) {
    bitmap_[frame_index / 8] &= ~(1 << (frame_index % 8));
}

bool PhysicalMemoryManager::testBit(uint64_t frame_index) const {
    return bitmap_[frame_index / 8] & (1 << (frame_index % 8));
}

void PhysicalMemoryManager::markRegionFree(uint64_t base, uint64_t length) {
    uint64_t start_frame = base / FRAME_SIZE;
    uint64_t end_frame = (base + length) / FRAME_SIZE;

    for (uint64_t frame = start_frame; frame < end_frame && frame < total_frames_; ++frame) {
        if (testBit(frame)) {
            clearBit(frame);
            --used_frames_;
        }
    }
}

void PhysicalMemoryManager::markRegionUsed(uint64_t base, uint64_t length) {
    uint64_t start_frame = base / FRAME_SIZE;
    uint64_t end_frame = (base + length + FRAME_SIZE - 1) / FRAME_SIZE;

    for (uint64_t frame = start_frame; frame < end_frame && frame < total_frames_; ++frame) {
        if (!testBit(frame)) {
            setBit(frame);
            ++used_frames_;
        }
    }
}

uint64_t PhysicalMemoryManager::allocFrame() {
    for (uint64_t i = 0; i < total_frames_; ++i) {
        uint64_t frame = (search_hint_ + i) % total_frames_;
        if (!testBit(frame)) {
            setBit(frame);
            ++used_frames_;
            search_hint_ = frame + 1;
            return frame * FRAME_SIZE;
        }
    }
    return UINT64_MAX;
}

void PhysicalMemoryManager::freeFrame(uint64_t phys_address) {
    uint64_t frame = phys_address / FRAME_SIZE;
    if (frame < total_frames_ && testBit(frame)) {
        clearBit(frame);
        --used_frames_;
    }
}
