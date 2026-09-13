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

#include <shell.h>
#include <terminal.h>
#include <keyboard.h>
#include <string.h>
#include <pmm.h>
#include <heap.h>
#include <pci.h>
#include <power.h>
#include <serial.h>

#include <cmos.h>
#include <rtc.h>
#include <ahci.h>
#include <vfs.h>

#include <ext2.h>
#include <timer.h>
#include <stdlib.h>
#include <scheduler.h>
#include <printf.h>

#include "elf.h"
#include "usermode.h"
#include <tar.h>

namespace Shell {
    namespace {
        constexpr size_t LINE_BUFFER_SIZE = 256;
        constexpr size_t MAX_ARGS = 16;
        constexpr size_t MAX_HISTORY = 16;

        char g_line_buffer[LINE_BUFFER_SIZE];
        size_t g_line_length = 0;
        size_t g_cursor = 0;
        size_t g_prev_render_length = 0;



        char g_history[MAX_HISTORY][LINE_BUFFER_SIZE];
        size_t g_history_lengths[MAX_HISTORY];
        size_t g_history_count = 0;
        size_t g_history_pos = 0;

        char g_elf_path[128];

        constexpr int MAX_ELF_ARGS = 16;
        char g_elf_argv_storage[MAX_ELF_ARGS][64];
        char *g_elf_argv[MAX_ARGS];
        int g_elf_argc = 0;

        void redraw() {
            g_terminal.setCursor(g_input_row, g_input_col);
            for (size_t i = 0; i < g_line_length; ++i) {
                g_terminal.putChar(g_line_buffer[i]);
            }
            for (size_t i = g_line_length; i < g_prev_render_length; ++i) {
                g_terminal.putChar(' ');
            }
            g_prev_render_length = g_line_length;
            g_terminal.setCursor(g_input_row, g_input_col + g_cursor);
        }

        void loadHistory(size_t index) {
            size_t len = g_history_lengths[index];
            for (size_t i = 0; i < len; ++i) g_line_buffer[i] = g_history[index][i];
            g_line_length = len;
            g_cursor = len;
        }

        void pushHistory(const char *line, size_t length) {
            if (length == 0) return;

            if (g_history_count < MAX_HISTORY) {
                for (size_t i = 0; i < length; ++i) g_history[g_history_count][i] = line[i];
                g_history_lengths[g_history_count] = length;
                ++g_history_count;
            } else {
                for (size_t h = 1; h < MAX_HISTORY; ++h) {
                    for (size_t i = 0; i < g_history_lengths[h]; ++i) g_history[h - 1][i] = g_history[h][i];
                    g_history_lengths[h - 1] = g_history_lengths[h];
                }
                for (size_t i = 0; i < length; ++i) g_history[MAX_HISTORY - 1][i] = line[i];
                g_history_lengths[MAX_HISTORY - 1] = length;
            }
            g_history_pos = g_history_count;
        }

        bool taskAlive(uint32_t id) {
            Scheduler::Task* start = Scheduler::currentTask();
            if (!start) return false;

            Scheduler::Task* t = start;
            do {
                if (t->id == id) return t->state != Scheduler::TaskState::Terminated;
                t = t->next;
            } while (t != start);

            return false;
        }

        void cmdHelp(int, char **) {
            g_terminal.write("Available commands:\n");
            g_terminal.write("  help        - show this message\n");
            g_terminal.write("  clear       - clear the screen\n");
            g_terminal.write("  echo        - print arguments\n");
            g_terminal.write("  meminfo     - show memory stats\n");
            g_terminal.write("  panic       - trigger a test CPU exception\n");
            g_terminal.write("  lspci       - list all PCI devices\n");
            g_terminal.write("  reboot      - reboot kernel\n");
            g_terminal.write("  time        - display current time using RTC\n");
            g_terminal.write("  date        - display current date using RTC\n");
            g_terminal.write("  serial      - print arguments to serial port\n");
            g_terminal.write("  lsahci      - list all identified AHCI ports\n");
            g_terminal.write("  identify    - identify type of AHCI port\n");
        }

        void cmdKill(int argc, char** argv) {
            if (argc < 2) {
                g_terminal.write("Usage: kill <task_id>\n");
                return;
            }

            uint32_t id = 0;
            for (int i = 0; argv[1][i] != '\0'; ++i) id = id * 10 + (argv[1][i] - '0');

            if (Scheduler::killTask(id, -1)) {
                g_terminal.write("Task killed.\n");
            } else {
                g_terminal.write("Kill failed (invalid ID, already dead, or last task).\n");
            }
        }

        void cmdPs(int, char**) {
            Scheduler::Task* start = Scheduler::currentTask();
            if (!start) {
                g_terminal.write("No tasks.\n");
                return;
            }

            Scheduler::Task* t = start;
            do {
                g_terminal.write("  [");
                g_terminal.writeDec(t->id);
                g_terminal.write("] ");
                switch (t->state) {
                    case Scheduler::TaskState::Ready:      g_terminal.write("Ready\n"); break;
                    case Scheduler::TaskState::Running:    g_terminal.write("Running\n"); break;
                    case Scheduler::TaskState::Blocked:    g_terminal.write("Blocked\n"); break;
                    case Scheduler::TaskState::Terminated: g_terminal.write("Terminated\n"); break;
                }
                t = t->next;
            } while (t != start);
        }

        void cmdClear(int argc, char **argv) {
            if (argc > 1) {
                if (argc == 2 && strcmp(argv[1], "all") == 0) {
                    g_terminal.clearAll();
                    return;
                }
            }
            g_terminal.clear();
        }

        void cmdEcho(int argc, char **argv) {
            for (int i = 1; i < argc; ++i) {
                g_terminal.write(argv[i]);
                if (i != argc - 1) g_terminal.write(" ");
            }
            g_terminal.write("\n");
        }

        void cmdLsPCI(int, char **) {
            for (int i = 0; i < PCI::get_index(); i++) {
                auto dev = PCI::getDevice(i);
                printf("%rPCI device %d: %r%x::%x::%x - %r%s%r\n", VGA::BROWN, i, VGA::LIGHT_BLUE, dev.bus, dev.slot, dev.func, VGA::LIGHT_GREEN, PCI::classCodeToString(dev.header.base_class, dev.header.sub_class), VGA::WHITE);
            }
        }

        void cmdLsahci(int, char**) {
            if (!AHCI::isInitialized()) {
                g_terminal.write("AHCI not initialized.\n");
                return;
            }
            for (int i = 0; i < AHCI::MAX_PORTS; ++i) {
                if (AHCI::portHasDevice(i)) {
                    g_terminal.write("Port ");
                    g_terminal.writeDec(i);
                    g_terminal.write(": device present\n");
                }
            }
        }

        void cmdReadTest(int argc, char** argv) {
            if (argc < 3) {
                g_terminal.write("Usage: readtest <port> <lba>\n");
                return;
            }

            int port = 0;
            for (int i = 0; argv[1][i] != '\0'; ++i) port = port * 10 + (argv[1][i] - '0');

            uint64_t lba = 0;
            for (int i = 0; argv[2][i] != '\0'; ++i) lba = lba * 10 + (argv[2][i] - '0');

            uint8_t buffer[512];
            if (!AHCI::readSectors(port, lba, 1, buffer)) {
                g_terminal.write("Read failed.\n");
                return;
            }

            g_terminal.write("First 32 bytes of sector ");
            g_terminal.writeDec(lba);
            g_terminal.write(":\n");
            for (int i = 0; i < 32; ++i) {
                g_terminal.writeHex(buffer[i]);
                g_terminal.write(" ");
            }
            g_terminal.write("\n");
        }

        void cmdMount(int argc, char** argv) {
            if (argc < 3) {
                g_terminal.write("Usage: mount <device> <mountpoint>\n");
                g_terminal.write("  e.g. mount sata0 /home\n");
                return;
            }

            if (!VFS::mount(argv[2], argv[1], Ext2::driver())) {
                g_terminal.write("Mount failed.\n");
                return;
            }

            printf("Mounted %s at %s\n", argv[1], argv[2]);
        }

        void cmdUmount(int argc, char** argv) {
            if (argc < 2) {
                g_terminal.write("Usage: umount <mountpoint>\n");
                return;
            }
            if (!VFS::unmount(argv[1])) {
                g_terminal.write("Umount failed (not mounted?).\n");
                return;
            }
            g_terminal.write("Unmounted ");
            g_terminal.write(argv[1]);
            g_terminal.write("\n");
        }

        void cmdTar(int, char **argv) {
            if (strcmp("ls", argv[1]) == 0) {
                char path[128];
                VFS::normalizePath(g_cwd, argv[2], path, 128);
                TAR::listFiles(path);
            } else if (strcmp("cat", argv[1]) == 0) {
                char path[128];
                VFS::normalizePath(g_cwd, argv[2], path, 128);
                TAR::catFile(path, argv[3]);
            } else if (strcmp("extract", argv[1]) == 0) {
                char path[128];
                char path_out[128];
                VFS::normalizePath(g_cwd, argv[2], path, 128);
                VFS::normalizePath(g_cwd, argv[4], path_out, 128);
                TAR::extractFile(path, argv[3], path_out);
            }
        }

        void cmdReboot(int argc, char **argv) {
            if (argc > 2) {
                g_terminal.write("Usage: reboot (hard)\n");
                return;
            }
            if (strcmp("hard", argv[1]) == 0) {
                Power::rebootResetVector();
                return;
            }
            Power::reboot();
        }

        void cmdMeminfo(int, char **) {
            g_terminal.write("Physical memory:\n");
            g_terminal.write("  Total frames: ");
            g_terminal.writeDec(g_pmm.totalFrames());
            g_terminal.write("\n");
            g_terminal.write("  Used frames:  ");
            g_terminal.writeDec(g_pmm.usedFrames());
            g_terminal.write("\n");
            g_terminal.write("  Free frames:  ");
            g_terminal.writeDec(g_pmm.freeFrames());
            g_terminal.write("\n");
            g_terminal.write("  Free memory:  ");
            g_terminal.writeDec(g_pmm.freeFrames() * PhysicalMemoryManager::FRAME_SIZE / 1024);
            g_terminal.write(" KB\n");

            g_terminal.write("Heap:\n");
            g_terminal.write("  Allocated: ");
            g_terminal.writeDec(Heap::totalAllocated());
            g_terminal.write(" bytes\n");
            g_terminal.write("  Free:      ");
            g_terminal.writeDec(Heap::totalFree());
            g_terminal.write(" bytes\n");
        }

        void cmdUptime(int, char **) {
            uint64_t ticks = Timer::getTicks();
            g_terminal.write("Ticks: ");
            g_terminal.writeDec(ticks);
            g_terminal.write(" (~");
            g_terminal.writeDec(ticks / 1000);
            g_terminal.write(" seconds)\n");
        }

        void cmdPanic(int, char **) {
            g_terminal.write("Triggering an exception...\n");
            asm volatile("int $3");
        }

        void cmdTime(int argc, char **argv) {
            if (argc > 2) {
                g_terminal.write("time - print current time in CEST or UTC\n");
                g_terminal.write("Usage: time (utc)\n");
                return;
            }
            bool utc{};
            int hours, minutes, seconds;
            if (strcmp("utc", argv[1]) == 0) utc = true;
            if (utc) {
                hours = RTC::readHours();
                minutes = RTC::readMinutes();
                seconds = RTC::readSeconds();
            } else {
                hours = RTC::readHours() + 2;
                minutes = RTC::readMinutes();
                seconds = RTC::readSeconds();
            }
            if (hours < 10) {
                g_terminal.write("0");
                g_terminal.writeDec(hours);
            } else {
                g_terminal.writeDec(hours);
            }
            g_terminal.write(":");
            if (minutes < 10) {
                g_terminal.write("0");
                g_terminal.writeDec(minutes);
            } else {
                g_terminal.writeDec(minutes);
            }
            g_terminal.write(":");
            if (seconds < 10) {
                g_terminal.write("0");
                g_terminal.writeDec(seconds);
            } else {
                g_terminal.writeDec(seconds);
            }
            g_terminal.write("\n");
        }

        void cmdLs(int argc, char** argv) {
            char target[128];
            const char* arg = (argc >= 2) ? argv[1] : ".";
            VFS::normalizePath(g_cwd, arg, target, sizeof(target));

            if (!VFS::isDirectory(target)) {
                g_terminal.write("Not a directory: ");
                g_terminal.write(target);
                g_terminal.write("\n");
                return;
            }

            VFS::DirEntry entry {};
            int index = 0;
            bool any = false;

            while (VFS::listDirectory(target, index, &entry)) {
                g_terminal.write(entry.name);
                if (entry.is_directory) g_terminal.write("/");
                g_terminal.write("\n");
                ++index;
                any = true;
            }

            if (!any) g_terminal.write("(empty)\n");
        }

        void cmdCd(const int argc, char** argv) {
            const char* arg = (argc >= 2) ? argv[1] : "/";
            char target[128];
            VFS::normalizePath(g_cwd, arg, target, sizeof(target));

            if (!VFS::isDirectory(target)) {
                g_terminal.write("No such directory: ");
                g_terminal.write(target);
                g_terminal.write("\n");
                return;
            }

            strncpy(g_cwd, target, sizeof(g_cwd) - 1);
            g_cwd[sizeof(g_cwd) - 1] = '\0';
        }

        void cmdPwd(int, char**) {
            g_terminal.write(g_cwd);
            g_terminal.write("\n");
        }

        void cmdDate(int, char **) {
            const int day = RTC::readDay();
            const int month = RTC::readMonth();
            const int year = RTC::readYear();
            const int century = RTC::readCentury();
            if (day < 10) {
                g_terminal.write("0");
                g_terminal.writeDec(day);
            } else {
                g_terminal.writeDec(day);
            }
            g_terminal.write(".");
            if (month < 10) {
                g_terminal.write("0");
                g_terminal.writeDec(month);
            } else {
                g_terminal.writeDec(month);
            }
            g_terminal.write(".");
            g_terminal.writeDec(century);
            g_terminal.writeDec(year);

            switch (CMOS::readRegister(RTC::REGISTER_DAY_OF_WEEK)) {
                case 1: g_terminal.write(" Sunday"); break;
                case 2: g_terminal.write(" Monday"); break;
                case 3: g_terminal.write(" Tuesday"); break;
                case 4: g_terminal.write(" Wednesday"); break;
                case 5: g_terminal.write(" Thursday"); break;
                case 6: g_terminal.write(" Friday"); break;
                case 7: g_terminal.write(" Saturday"); break;
                default: g_terminal.write("Error while reading day from RTC"); break;
            }

            g_terminal.write("\n");
        }

        void cmdMemDump(const int argc, char **argv) {
            if (argc < 3) {
                g_terminal.write("Usage: memdump <address> <bytes>\n");
                return;
            }

            uint64_t addr;
            hex_to_u64(argv[1], &addr);
            const uint64_t bytes = atoll(argv[2]);

            constexpr uint64_t bytesPerLine = 14;
            const char hexChars[] = "0123456789ABCDEF";

            for (uint64_t i = 0; i < bytes; i += bytesPerLine) {
                g_terminal.setColorForeground(VGA::BLUE);
                g_terminal.writeHex(addr + i);
                g_terminal.setColorForeground(VGA::WHITE);
                g_terminal.write(": ");
                g_terminal.setColorForeground(VGA::GREEN);
                for (uint64_t j = 0; j < bytesPerLine; j++) {
                    if (i + j < bytes) {
                        uint8_t value = *reinterpret_cast<volatile uint8_t *>(addr + i + j);

                        char hexBuf[4] = {
                            hexChars[(value >> 4) & 0xF],
                            hexChars[value & 0xF],
                            ' ',
                            '\0'
                        };
                        g_terminal.write(hexBuf);
                    } else {
                        g_terminal.write("   ");
                    }
                }
                g_terminal.setColorForeground(VGA::WHITE);
                g_terminal.write(" |");

                g_terminal.setColorForeground(VGA::RED);
                for (uint64_t j = 0; j < bytesPerLine; j++) {
                    if (i + j < bytes) {
                        uint8_t value = *reinterpret_cast<volatile uint8_t *>(addr + i + j);
                        char c = (value >= 32 && value <= 126) ? static_cast<char>(value) : '.';
                        char s[2] = { c, '\0' };
                        g_terminal.write(s);
                    } else {
                        g_terminal.write(" ");
                    }
                }
                g_terminal.setColorForeground(VGA::WHITE);
                g_terminal.write("|\n");
                Timer::sleepMiliseconds(50);
            }
        }



        void cmdLsDisks(int, char**) {
            int count = VFS::blockDeviceCount();
            if (count == 0) {
                g_terminal.write("No block devices registered.\n");
                return;
            }
            for (int i = 0; i < count; ++i) {
                const VFS::BlockDevice* dev = VFS::getBlockDevice(i);
                g_terminal.write("/dev/disks/");
                g_terminal.write(dev->name);
                g_terminal.write("  ");
                g_terminal.writeDec(dev->sector_count);
                g_terminal.write(" sectors (~");
                g_terminal.writeDec((dev->sector_count * dev->sector_size) / (1024 * 1024));
                g_terminal.write(" MB)\n");
            }
        }

        void cmdMounts(int, char**) {
            int count = VFS::mountCount();
            if (count == 0) {
                g_terminal.write("No filesystems mounted.\n");
                return;
            }
            for (int i = 0; i < count; ++i) {
                const VFS::MountInfo* m = VFS::getMount(i);
                g_terminal.write(m->mount_point);
                g_terminal.write("  <- ");
                g_terminal.write(m->device_name);
                g_terminal.write(" (");
                g_terminal.write(m->fs_name);
                g_terminal.write(")\n");
            }
        }

        void cmdInitAHCI(int, char **) {
            AHCI::init();
        }

        void cmdTouch(int argc, char** argv) {
            if (argc < 2) { g_terminal.write("Usage: touch <path>\n"); return; }
            char target[128];
            VFS::normalizePath(g_cwd, argv[1], target, sizeof(target));
            g_terminal.write(VFS::create(target, false) ? "Created.\n" : "Failed.\n");
        }

        void cmdMkdir(int argc, char** argv) {
            if (argc < 2) { g_terminal.write("Usage: mkdir <path>\n"); return; }
            char target[128];
            VFS::normalizePath(g_cwd, argv[1], target, sizeof(target));
            g_terminal.write(VFS::create(target, true) ? "Created.\n" : "Failed.\n");
        }

        void cmdWriteFile(int argc, char** argv) {
            if (argc < 3) { g_terminal.write("Usage: writefile <path> <text>\n"); return; }
            char target[128];
            VFS::normalizePath(g_cwd, argv[1], target, sizeof(target));

            int fd = VFS::open(target);
            if (fd == VFS::INVALID_FD) {
                if (!VFS::create(target, false)) { g_terminal.write("Create failed.\n"); return; }
                fd = VFS::open(target);
                if (fd == VFS::INVALID_FD) { g_terminal.write("Open failed.\n"); return; }
            }

            int64_t n = VFS::write(fd, argv[2], strlen(argv[2]));
            VFS::close(fd);

            g_terminal.write("Wrote ");
            g_terminal.writeDec(n);
            g_terminal.write(" bytes.\n");
        }

        void cmdCat(int argc, char** argv) {
            if (argc < 2) {
                g_terminal.write("Usage: cat <path>\n");
                return;
            }

            char target[128];
            VFS::normalizePath(g_cwd, argv[1], target, sizeof(target));

            int fd = VFS::open(target);
            if (fd == VFS::INVALID_FD) {
                g_terminal.write("cat: cannot open ");
                g_terminal.write(target);
                g_terminal.write("\n");
                return;
            }

            char buf[257];
            int64_t n;

            while ((n = VFS::read(fd, buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                g_terminal.write(buf);
            }

            if (n < 0) {
                g_terminal.write("\ncat: read error\n");
            }

            VFS::close(fd);

            g_terminal.write("\n");
        }

        void cmdPciInfo(int argc, char **argv) {
            int id = atoi(argv[1]);
            PCI::PCIDevice dev = PCI::getDevice(id);
            g_terminal.write("PCI device ");
            g_terminal.writeDec(id);
            g_terminal.setColor(15, 0);
            g_terminal.write(": \n  Vendor ID: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.vendor_id);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Device ID: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.device_id);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  Command: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.command_register);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Status: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.status_register);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  Class: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.base_class);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Subclass: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.sub_class);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Func: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.prog_if);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Revision: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.revision_id);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  BIST: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.bist);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Header: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.header_type);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Latency timer: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.latency_timer);
            g_terminal.setColor(15, 0);
            g_terminal.write("  Cache line: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.cache_line_size);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  BAR0: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.BAR0);
            g_terminal.setColor(15, 0);
            g_terminal.write("  BAR1: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.BAR1);
            g_terminal.setColor(15, 0);
            g_terminal.write("  BAR2: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.BAR2);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  BAR3: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.BAR3);
            g_terminal.setColor(15, 0);
            g_terminal.write("  BAR4: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.BAR4);
            g_terminal.setColor(15, 0);
            g_terminal.write("  BAR5: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.BAR5);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  CIS Pointer: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.cardbus_cis_pointer);
            g_terminal.setColor(15, 0);
            g_terminal.write("  SubVen ID: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.subsystem_vendor_id);
            g_terminal.setColor(15, 0);
            g_terminal.write("  SubDev ID: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.subsystem_device_id);
            g_terminal.setColor(15, 0);
            g_terminal.write("  ERBA: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.ex_rba);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n  Capabilities: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.capabilities_pointer);
            g_terminal.setColor(15, 0);
            g_terminal.write("  INT Line: ");
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.interrupt_line);
            g_terminal.write("  INT PIN: ");
            g_terminal.setColor(15, 0);
            g_terminal.setColor(2, 0);
            g_terminal.writeHex(dev.header.interrupt_pin);
            g_terminal.setColor(15, 0);
            g_terminal.write("\n");
        }

        void cmdRm(int argc, char** argv) {
            if (argc < 2) { g_terminal.write("Usage: rm <path>\n"); return; }
            char target[128];
            VFS::normalizePath(g_cwd, argv[1], target, sizeof(target));
            g_terminal.write(VFS::unlink(target) ? "Removed.\n" : "Failed.\n");
        }

        void cmdRmdir(int argc, char** argv) {
            if (argc < 2) { g_terminal.write("Usage: rmdir <path>\n"); return; }
            char target[128];
            VFS::normalizePath(g_cwd, argv[1], target, sizeof(target));
            g_terminal.write(VFS::rmdir(target) ? "Removed.\n" : "Failed (not empty or not found).\n");
        }

        void cmdIdentify(int argc, char** argv) {
            if (argc < 2) {
                g_terminal.write("Usage: identify <port>\n");
                return;
            }

            int port = 0;
            for (int i = 0; argv[1][i] != '\0'; ++i) {
                port = port * 10 + (argv[1][i] - '0');
            }

            uint16_t buf[256];
            if (!AHCI::identify(port, buf)) {
                g_terminal.write("IDENTIFY failed.\n");
                return;
            }

            char model[41];
            for (int i = 0; i < 20; ++i) {
                uint16_t word = buf[27 + i];
                model[i * 2]     = static_cast<char>((word >> 8) & 0xFF);
                model[i * 2 + 1] = static_cast<char>(word & 0xFF);
            }
            model[40] = '\0';

            g_terminal.write("Model: ");
            g_terminal.write(model);
            g_terminal.write("\n");

            uint32_t sectors = static_cast<uint32_t>(buf[60]) | (static_cast<uint32_t>(buf[61]) << 16);
            g_terminal.write("Sectors (28-bit): ");
            g_terminal.writeDec(sectors);
            g_terminal.write(" (~");
            g_terminal.writeDec((static_cast<uint64_t>(sectors) * 512) / (1024 * 1024));
            g_terminal.write(" MB)\n");
        }

        void cmdSerial(int argc, char **argv) {
            if (argc < 2) {
                g_terminal.write("Usage: serial <message>\n");
                return;
            }
            for (int i = 1; i < argc; ++i) {
                Serial::write(argv[i]);
                if (i != argc - 1) Serial::write(" ");
            }
            Serial::write("\n");
            g_terminal.write("Sent to serial.\n");
        }

        struct Command {
            const char *name;

            void (*handler)(int argc, char **argv);
        };

        void userTaskEntry() {
            uint64_t stack_top = 0;
            uint64_t argv_user_ptr = 0;

            uint64_t entry = Elf::load(g_elf_path, g_elf_argc, g_elf_argv, &stack_top, &argv_user_ptr);

            if (!entry) {
                printf("%rFailed to load ELF executable%r\n", VGA::RED, VGA::WHITE);
                Keyboard::setFocus(0);
                Scheduler::exitTask();
                __builtin_unreachable();
            }

            Usermode::enter(entry, stack_top, g_elf_argc, argv_user_ptr);
        }

        void cmdExecuteElf(int argc, char** argv) {
            if (argc < 2) {
                printf("Usage: execelf <path> [args...]\n");
                return;
            }

            VFS::normalizePath(g_cwd, argv[1], g_elf_path, sizeof(g_elf_path));

            g_elf_argc = 0;
            strncpy(g_elf_argv_storage[g_elf_argc], g_elf_path, sizeof(g_elf_argv_storage[0]) - 1);
            g_elf_argv[g_elf_argc] = g_elf_argv_storage[g_elf_argc];
            ++g_elf_argc;

            for (int i = 2; i < argc && g_elf_argc < MAX_ELF_ARGS; ++i) {
                strncpy(g_elf_argv_storage[g_elf_argc], argv[i], sizeof(g_elf_argv_storage[0]) - 1);
                g_elf_argv_storage[g_elf_argc][sizeof(g_elf_argv_storage[0]) - 1] = '\0';
                g_elf_argv[g_elf_argc] = g_elf_argv_storage[g_elf_argc];
                ++g_elf_argc;
            }

            Scheduler::Task* task = Scheduler::createTask(userTaskEntry);
            if (task) {
                Keyboard::setFocus(task->id);
            }

            uint32_t task_id = task->id;

            int32_t exit_code = Scheduler::waitForTask(task_id);
            printf("[shell] exited with code %d\n", exit_code);

            while (taskAlive(task_id)) {
                Scheduler::yield();
            }
        }

        const Command g_commands[] = {
            {"help", cmdHelp},
            {"clear", cmdClear},
            {"echo", cmdEcho},
            {"meminfo", cmdMeminfo},
            {"panic", cmdPanic},
            {"lspci", cmdLsPCI},
            {"reboot", cmdReboot},
            {"time", cmdTime},
            {"date", cmdDate},
            {"serial", cmdSerial},
            {"lsahci", cmdLsahci},
            {"identify", cmdIdentify},
            {"readtest", cmdReadTest},
            { "lsdisks", cmdLsDisks },
            { "mounts", cmdMounts },
            { "ls",  cmdLs },
            { "cd",  cmdCd },
            { "pwd", cmdPwd },
            {"mount", cmdMount},
            {"umount", cmdUmount},
            {"cat", cmdCat},
            {"uptime", cmdUptime},
            {"initahci", cmdInitAHCI},
            {"memdump", cmdMemDump},
            {"kill", cmdKill},
            {"ps", cmdPs},
            {"touch", cmdTouch},
            {"mkdir", cmdMkdir},
            {"writefile", cmdWriteFile},
            {"pci_info", cmdPciInfo},
            {"rm", cmdRm},
            {"rmdir", cmdRmdir},
            {"execelf", cmdExecuteElf},
            {"tar", cmdTar}
        };

        constexpr size_t COMMAND_COUNT = sizeof(g_commands) / sizeof(g_commands[0]);

        int tokenize(char *line, char **argv, int max_args) {
            int argc = 0;
            char *p = line;

            while (*p != '\0' && argc < max_args) {
                while (*p == ' ') ++p;
                if (*p == '\0') break;

                argv[argc++] = p;

                while (*p != '\0' && *p != ' ') ++p;
                if (*p == ' ') {
                    *p = '\0';
                    ++p;
                }
            }
            return argc;
        }

        void executeLine(char *line) {
            char *argv[MAX_ARGS];
            int argc = tokenize(line, argv, MAX_ARGS);

            if (argc == 0) return;

            for (size_t i = 0; i < COMMAND_COUNT; ++i) {
                if (strcmp(argv[0], g_commands[i].name) == 0) {
                    g_commands[i].handler(argc, argv);
                    return;
                }
            }

            g_terminal.write("Unknown command: ");
            g_terminal.write(argv[0]);
            g_terminal.write("\n");
        }
    }

    char g_cwd[128] = "/";

    void printPrompt() {
        g_terminal.write("WengShiOS:");
        g_terminal.write(g_cwd);
        g_terminal.write("> ");
        g_input_row = g_terminal.getRow();
        g_input_col = g_terminal.getColumn();
        g_prev_render_length = 0;
    }

    void init() {
        g_line_length = 0;
        g_cursor = 0;
        g_history_count = 0;
        g_history_pos = 0;
    }

    [[noreturn]] void run() {
        g_terminal.write("\nWengShiOS Shell - type 'help' for a list of commands\n");
        printPrompt();

        for (;;) {
            if (Keyboard::hasChar()) {
                char c = Keyboard::getChar();

                if (c == '\n') {
                    g_terminal.setCursor(g_input_row, g_input_col + g_line_length);
                    g_terminal.putChar('\n');

                    g_line_buffer[g_line_length] = '\0';
                    pushHistory(g_line_buffer, g_line_length);

                    executeLine(g_line_buffer);

                    g_line_length = 0;
                    g_cursor = 0;
                    g_terminal.write("\n");
                    printPrompt();
                } else if (c == '\b') {
                    if (g_cursor > 0) {
                        for (size_t i = g_cursor - 1; i < g_line_length - 1; ++i) {
                            g_line_buffer[i] = g_line_buffer[i + 1];
                        }
                        --g_line_length;
                        --g_cursor;
                        redraw();
                    }
                } else if (c == Keyboard::KEY_LEFT) {
                    if (g_cursor > 0) {
                        --g_cursor;
                        g_terminal.setCursor(g_input_row, g_input_col + g_cursor);
                    }
                } else if (c == Keyboard::KEY_RIGHT) {
                    if (g_cursor < g_line_length) {
                        ++g_cursor;
                        g_terminal.setCursor(g_input_row, g_input_col + g_cursor);
                    }
                } else if (c == Keyboard::KEY_UP) {
                    if (g_history_pos > 0) {
                        --g_history_pos;
                        loadHistory(g_history_pos);
                        redraw();
                    }
                } else if (c == Keyboard::KEY_DOWN) {
                    if (g_history_pos < g_history_count) {
                        ++g_history_pos;
                        if (g_history_pos == g_history_count) {
                            g_line_length = 0;
                            g_cursor = 0;
                        } else {
                            loadHistory(g_history_pos);
                        }
                        redraw();
                    }
                } else if (g_line_length < LINE_BUFFER_SIZE - 1) {
                    for (size_t i = g_line_length; i > g_cursor; --i) {
                        g_line_buffer[i] = g_line_buffer[i - 1];
                    }
                    g_line_buffer[g_cursor] = c;
                    ++g_line_length;
                    ++g_cursor;
                    redraw();
                }
            }
            asm volatile("hlt");
        }
    }

    uint16_t g_input_row = 0;
    uint16_t g_input_col = 0;
}
