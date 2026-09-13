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

#include <rtc.h>
#include <cmos.h>

namespace RTC {
    namespace {
        int bcdToInt(const uint8_t bcd) {
            return (bcd & 0x0F) + ((bcd >> 4) * 10);
        }
    }
    bool dataMode() {
        const uint8_t b_register = CMOS::readRegister(REGISTER_B);

        if (const uint8_t data_mode_bit = (b_register & 0b100) >> 2; data_mode_bit == 1) {
            return true;
        }
        return false;
    }

    int readHours() {
        const uint8_t reg_hour = CMOS::readRegister(REGISTER_HOURS);
        if (dataMode()) {
            return static_cast<int>(reg_hour);
        }
        return bcdToInt(reg_hour);
    }

    int readMinutes() {
        const uint8_t reg_min = CMOS::readRegister(REGISTER_MINUTES);
        if (dataMode()) {
            return static_cast<int>(reg_min);
        }
        return bcdToInt(reg_min);
    }

    int readSeconds() {
        const uint8_t reg_sec = CMOS::readRegister(REGISTER_SECONDS);
        if (dataMode()) {
            return static_cast<int>(reg_sec);
        }
        return bcdToInt(reg_sec);
    }

    int readDay() {
        const uint8_t reg_day = CMOS::readRegister(REGISTER_DAY_OF_MONTH);
        if (dataMode()) {
            return static_cast<int>(reg_day);
        }
        return bcdToInt(reg_day);
    }

    int readMonth() {
        const uint8_t reg_month = CMOS::readRegister(REGISTER_MONTH);
        if (dataMode()) {
            return static_cast<int>(reg_month);
        }
        return bcdToInt(reg_month);
    }

    int readYear() {
        const uint8_t reg_year = CMOS::readRegister(REGISTER_YEAR);
        if (dataMode()) {
            return static_cast<int>(reg_year);
        }
        return bcdToInt(reg_year);
    }

    int readCentury() {
        const uint8_t reg_cen = CMOS::readRegister(REGISTER_CENTURY);
        if (dataMode()) {
            return static_cast<int>(reg_cen);
        }
        return bcdToInt(reg_cen);
    }

    int readDayInWeek() {
        const uint8_t reg_cen = CMOS::readRegister(REGISTER_DAY_OF_WEEK);
        if (dataMode()) {
            return static_cast<int>(reg_cen);
        }
        return bcdToInt(reg_cen);
    }
}
