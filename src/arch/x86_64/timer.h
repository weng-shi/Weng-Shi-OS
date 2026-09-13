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

namespace Timer {
    void init(uint32_t frequency_hz);
    uint64_t getTicks();
    void onIRQ();
    void sleepTicks(uint64_t sec);
    void sleepSeconds(uint64_t s);
    void sleepMiliseconds(uint64_t ms);
}