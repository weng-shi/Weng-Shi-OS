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

#include <stdarg.h>
#include <stdint.h>
#include <terminal.h>

void printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            switch (format[i]) {
                case 's': {
                    const char *str = va_arg(args, const char *);
                    if (!str) str = "(null)";
                    g_terminal.write(str);
                    break;
                }
                case 'c': {
                    char c = static_cast<char>(va_arg(args, int));
                    char buf[2] = {c, '\0'};
                    g_terminal.write(buf);
                    break;
                }
                case 'r': {
                    int fg = va_arg(args, int);
                    g_terminal.setColorForeground(fg); break;
                }
                case 'R': {
                    int bg = va_arg(args, int);
                    g_terminal.setColorBackground(bg); break;
                }
                case 'd':
                case 'u': {
                    uint64_t val = (format[i] == 'd') ? 
                        (uint64_t)va_arg(args, int64_t) : 
                        va_arg(args, uint64_t);

                    if (format[i] == 'd' && static_cast<int64_t>(val) < 0) {
                        g_terminal.write("-");
                        val = static_cast<uint64_t>(-static_cast<int64_t>(val));
                    }
                    g_terminal.writeDec(val);
                    break;
                }
                case 'x':
                case 'p': {
                    if (format[i] == 'p') {
                        g_terminal.write("0x");
                    }
                    uint64_t val = va_arg(args, uint64_t);
                    g_terminal.writeHex(val);
                    break;
                }
                case 'b': {
                    uint64_t val = va_arg(args, uint64_t);
                    if (val == 0) {
                        g_terminal.write("0");
                    } else {
                        char buf[65];
                        buf[64] = '\0';
                        int idx = 63;
                        while (val > 0) {
                            buf[idx] = (val & 1) ? '1' : '0';
                            val >>= 1;
                            idx--;
                        }
                        g_terminal.write(&buf[idx + 1]);
                    }
                    break;
                }
                case 'o': {
                    uint64_t val = va_arg(args, uint64_t);
                    if (val == 0) {
                        g_terminal.write("0");
                    } else {
                        char buf[23];
                        buf[22] = '\0';
                        int idx = 21;
                        while (val > 0) {
                            buf[idx] = '0' + (val & 7);
                            val >>= 3;
                            idx--;
                        }
                        g_terminal.write(&buf[idx + 1]);
                    }
                    break;
                }
                case '%': {
                    g_terminal.write("%");
                    break;
                }
                default: {
                    char buf[3] = {'%', format[i], '\0'};
                    g_terminal.write(buf);
                    break;
                }
            }
        } else {
            char buf[2] = {format[i], '\0'};
            g_terminal.write(buf);
        }
    }

    va_end(args);
}