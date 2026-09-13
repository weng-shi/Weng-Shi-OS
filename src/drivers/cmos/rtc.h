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

namespace RTC {
    constexpr uint8_t REGISTER_SECONDS = 0x00;
    constexpr uint8_t REGISTER_MINUTES = 0x02;
    constexpr uint8_t REGISTER_HOURS = 0x04;
    constexpr uint8_t REGISTER_DAY_OF_WEEK = 0x06;
    constexpr uint8_t REGISTER_DAY_OF_MONTH = 0x07;
    constexpr uint8_t REGISTER_MONTH = 0x08;
    constexpr uint8_t REGISTER_YEAR = 0x09;
    constexpr uint8_t REGISTER_B = 0x0B;
    constexpr uint8_t REGISTER_CENTURY = 0x32;

    bool dataMode();

    int readHours();
    int readMinutes();
    int readSeconds();

    int readDay();
    int readMonth();
    int readYear();
    int readCentury();
    int readDayInWeek();
}
