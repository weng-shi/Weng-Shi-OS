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

#include <heap.h>
#include <vmm.h>
#include <terminal.h>
#include <serial.h>

namespace Heap {

  namespace {

    constexpr uint64_t HEAP_START = 0xFFFFFFFFC0000000ull;
    constexpr uint64_t HEAP_INITIAL_SIZE = 64 * 1024;
    constexpr uint64_t HEAP_MAX_SIZE     = 64 * 1024 * 1024;

    struct BlockHeader {
      size_t size;
      bool free;
      BlockHeader* next;
      BlockHeader* prev;
    };

    constexpr size_t HEADER_SIZE = sizeof(BlockHeader);
    constexpr size_t ALIGNMENT = 16;

    BlockHeader* g_first_block = nullptr;
    uint64_t g_heap_current_end = HEAP_START;
    uint64_t g_heap_mapped_end = HEAP_START;

    size_t alignUp(size_t value, size_t align) {
      return (value + align - 1) & ~(align - 1);
    }

    bool growHeap(size_t min_bytes) {
      uint64_t bytes_needed = alignUp(min_bytes, VMM::PAGE_SIZE);
      if (g_heap_mapped_end + bytes_needed > HEAP_START + HEAP_MAX_SIZE) {
        return false;
      }

      uint64_t pages_needed = bytes_needed / VMM::PAGE_SIZE;
      for (uint64_t i = 0; i < pages_needed; ++i) {
        Serial::write("growHeap: mapping page "); Serial::writeDec(i); Serial::write("\n");
        if (!VMM::allocAndMapPage(g_heap_mapped_end, VMM::PAGE_WRITABLE)) {
          return false;
        }
        Serial::write("growHeap: mapped ok\n");
        g_heap_mapped_end += VMM::PAGE_SIZE;
      }
      return true;
    }

  }

  void init() {
    if (!growHeap(HEAP_INITIAL_SIZE)) {
      g_terminal.write("Heap: FATAL — could not allocate initial region!\n");
      for (;;) asm volatile("hlt");
    }

    g_first_block = reinterpret_cast<BlockHeader*>(HEAP_START);
    g_first_block->size = HEAP_INITIAL_SIZE - HEADER_SIZE;
    g_first_block->free = true;
    g_first_block->next = nullptr;
    g_first_block->prev = nullptr;

    g_heap_current_end = g_heap_mapped_end;
  }

  void* kmalloc(size_t size) {
    if (size == 0) return nullptr;
    size = alignUp(size, ALIGNMENT);

    BlockHeader* block = g_first_block;
    while (block) {
      if (block->free && block->size >= size) {
        if (block->size >= size + HEADER_SIZE + ALIGNMENT) {
          BlockHeader* new_block = reinterpret_cast<BlockHeader*>(
                                                                  reinterpret_cast<uint8_t*>(block) + HEADER_SIZE + size);
          new_block->size = block->size - size - HEADER_SIZE;
          new_block->free = true;
          new_block->next = block->next;
          new_block->prev = block;
          if (block->next) block->next->prev = new_block;
          block->next = new_block;
          block->size = size;
        }

        block->free = false;
        return reinterpret_cast<uint8_t*>(block) + HEADER_SIZE;
      }
      block = block->next;
    }

    size_t grow_amount = size + HEADER_SIZE;
    if (grow_amount < 4096) grow_amount = 4096;

    uint64_t new_block_addr = g_heap_mapped_end;
    if (!growHeap(grow_amount)) {
      g_terminal.write("kmalloc: out of heap space!\n");
      return nullptr;
    }

    BlockHeader* new_block = reinterpret_cast<BlockHeader*>(new_block_addr);
    new_block->size = (g_heap_mapped_end - new_block_addr) - HEADER_SIZE;
    new_block->free = true;
    new_block->next = nullptr;
    new_block->prev = nullptr;

    BlockHeader* tail = g_first_block;
    while (tail->next) tail = tail->next;
    tail->next = new_block;
    new_block->prev = tail;

    return kmalloc(size);
  }

  void kfree(void* ptr) {
    if (!ptr) return;

    BlockHeader* block = reinterpret_cast<BlockHeader*>(
                                                        reinterpret_cast<uint8_t*>(ptr) - HEADER_SIZE);
    block->free = true;

    if (block->next && block->next->free) {
      BlockHeader* next = block->next;
      block->size += HEADER_SIZE + next->size;
      block->next = next->next;
      if (next->next) next->next->prev = block;
    }

    if (block->prev && block->prev->free) {
      BlockHeader* prev = block->prev;
      prev->size += HEADER_SIZE + block->size;
      prev->next = block->next;
      if (block->next) block->next->prev = prev;
    }
  }

  size_t totalAllocated() {
    size_t total = 0;
    for (BlockHeader* b = g_first_block; b; b = b->next)
      if (!b->free) total += b->size;
    return total;
  }

  size_t totalFree() {
    size_t total = 0;
    for (BlockHeader* b = g_first_block; b; b = b->next)
      if (b->free) total += b->size;
    return total;
  }

}
