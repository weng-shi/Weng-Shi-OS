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

#include <stddef.h>
#include <heap.h>

void* operator new(size_t size) {
    return Heap::kmalloc(size);
}

void* operator new[](size_t size) {
    return Heap::kmalloc(size);
}

void operator delete(void* ptr) noexcept {
    Heap::kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
    Heap::kfree(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    Heap::kfree(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    Heap::kfree(ptr);
}

inline void* operator new(size_t, void* ptr) noexcept { return ptr; }
inline void* operator new[](size_t, void* ptr) noexcept { return ptr; }