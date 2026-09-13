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

#include <stdint-gcc.h>
#include <stdlib.h>

int atoi(const char *s) {
    int sign = 1;
    int result = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\v' || *s == '\f') {
        s++;
           }

    if (*s == '+' || *s == '-') {
        if (*s == '-') sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    return sign * result;
}

long atol(const char *s) {
    long sign = 1;
    long result = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\v' || *s == '\f') {
        s++;
           }

    if (*s == '+' || *s == '-') {
        if (*s == '-') sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    return sign * result;
}

long long atoll(const char *s) {
    long long sign = 1;
    long long result = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\v' || *s == '\f') {
        s++;
           }

    if (*s == '+' || *s == '-') {
        if (*s == '-') sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    return sign * result;
}

static int is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\n' ||
            c == '\r' || c == '\v' || c == '\f');
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

int hex_to_u64(const char *s, uint64_t *out) {
    uint64_t acc = 0;
    int d;

    if (!s || !out) return -1;

    while (is_space(*s)) s++;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    if (hex_digit(*s) < 0) return -1;

    while ((d = hex_digit(*s)) >= 0) {
        if (acc > (UINT64_MAX >> 4)) return -1;
        acc = (acc << 4) | (uint64_t)d;
        s++;
    }

    *out = acc;
    return 0;
}