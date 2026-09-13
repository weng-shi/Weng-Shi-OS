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

#include <statusbar.h>
#include <rtc.h>
#include <terminal.h>

#include "timer.h"

namespace StatusBar {
    void date() {
        for (;;) {
            const int year = RTC::readYear();
            const int century = RTC::readCentury();
            const int month = RTC::readMonth();
            const int day = RTC::readDay();
            g_terminal.setColorForeground(VGA::LIGHT_BLUE);
            g_terminal.writeDecAt(year, STATUS_BAR_POSITION_END - 1, STATUS_BAR_Y);
            g_terminal.writeDecAt(century, STATUS_BAR_POSITION_END - 3, STATUS_BAR_Y);
            g_terminal.putCharAt('.', STATUS_BAR_POSITION_END - 4, STATUS_BAR_Y);
            if (month < 10) {
                g_terminal.writeDecAt(month, STATUS_BAR_POSITION_END - 5, STATUS_BAR_Y);
                g_terminal.putCharAt('0', STATUS_BAR_POSITION_END - 6, STATUS_BAR_Y);
            } else {
                g_terminal.writeDecAt(month, STATUS_BAR_POSITION_END - 6, STATUS_BAR_Y);
            }
            g_terminal.putCharAt('.', STATUS_BAR_POSITION_END - 7, STATUS_BAR_Y);
            if (day < 10) {
                g_terminal.writeDecAt(day, STATUS_BAR_POSITION_END - 8, STATUS_BAR_Y);
                g_terminal.putCharAt('0', STATUS_BAR_POSITION_END - 9, STATUS_BAR_Y);
            } else {
                g_terminal.writeDecAt(day, STATUS_BAR_POSITION_END - 9, STATUS_BAR_Y);
            }
            g_terminal.setColorForeground(VGA::WHITE);
            Timer::sleepMiliseconds(200);
        }
    }

    [[noreturn]] void time() {
        for (;;) {
            const int hour = RTC::readHours();
            const int minutes = RTC::readMinutes();
            const int seconds = RTC::readSeconds();
            g_terminal.setColorForeground(VGA::GREEN);
            if (seconds < 10) {
                g_terminal.writeDecAt(seconds, STATUS_BAR_POSITION_END - 11, STATUS_BAR_Y);
                g_terminal.putCharAt('0', STATUS_BAR_POSITION_END - 12, STATUS_BAR_Y);
            } else {
                g_terminal.writeDecAt(seconds, STATUS_BAR_POSITION_END - 12, STATUS_BAR_Y);
            }
            g_terminal.putCharAt(':', STATUS_BAR_POSITION_END - 13, STATUS_BAR_Y);
            if (minutes < 10) {
                g_terminal.writeDecAt(minutes, STATUS_BAR_POSITION_END - 14, STATUS_BAR_Y);
                g_terminal.putCharAt('0', STATUS_BAR_POSITION_END - 15, STATUS_BAR_Y);
            } else {
                g_terminal.writeDecAt(minutes, STATUS_BAR_POSITION_END - 15, STATUS_BAR_Y);
            }
            g_terminal.putCharAt(':', STATUS_BAR_POSITION_END - 16, STATUS_BAR_Y);
            if (hour < 8) {
                g_terminal.writeDecAt(hour + 2, STATUS_BAR_POSITION_END - 17, STATUS_BAR_Y);
                g_terminal.putCharAt('0', STATUS_BAR_POSITION_END - 18, STATUS_BAR_Y);
            } else {
                g_terminal.writeDecAt(hour + 2, STATUS_BAR_POSITION_END - 18, STATUS_BAR_Y);
            }
            g_terminal.setColorForeground(VGA::WHITE);
            Timer::sleepMiliseconds(200);
        }
    }
}
