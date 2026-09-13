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

#include <string.h>

extern "C" {

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < count; ++i) d[i] = s[i];
    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    if (d == s || count == 0) return dest;

    if (d < s) {
        for (size_t i = 0; i < count; ++i) d[i] = s[i];
    } else {
        for (size_t i = count; i > 0; --i) d[i - 1] = s[i - 1];
    }
    return dest;
}

void* memset(void* dest, int value, size_t count) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    uint8_t v = static_cast<uint8_t>(value);
    for (size_t i = 0; i < count; ++i) d[i] = v;
    return dest;
}

int memcmp(const void* a, const void* b, size_t count) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < count; ++i) {
        if (pa[i] != pb[i]) return static_cast<int>(pa[i]) - static_cast<int>(pb[i]);
    }
    return 0;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') ++len;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

int strncmp(const char* a, const char* b, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (a[i] != b[i] || a[i] == '\0') {
            return static_cast<unsigned char>(a[i]) - static_cast<unsigned char>(b[i]);
        }
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* original = dest;
    while ((*dest++ = *src++) != '\0') {}
    return original;
}

char* strncpy(char* dest, const char* src, size_t count) {
    size_t i = 0;
    for (; i < count && src[i] != '\0'; ++i) dest[i] = src[i];
    for (; i < count; ++i) dest[i] = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* original = dest;
    while (*dest) ++dest;
    while ((*dest++ = *src++) != '\0') {}
    return original;
}

const char* strchr(const char* str, int ch) {
    while (*str) {
        if (*str == static_cast<char>(ch)) return str;
        ++str;
    }
    return (ch == '\0') ? str : nullptr;
}

}