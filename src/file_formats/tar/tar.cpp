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

#include <tar.h>
#include <vfs.h>
#include <heap.h>
#include <string.h>
#include <printf.h>
#include <colors.h>

#include "pci.h"
#include "timer.h"

namespace TAR {
    namespace {
        struct USTAR_Hdr {
            char t_file_name[100];
            char t_file_mode[8];
            char t_uid[8];
            char t_gid[8];
            char t_file_size[12];
            char t_last_mod_time[12];
            char t_check_sum[8];
            char t_type_flag;
            char t_link_name[100];
            char t_magic[6];
            char t_version[2];
            char t_owner_name[32];
            char t_group_name[32];
            char t_dev_major[8];
            char t_dev_minor[8];
            char t_prefix[155];
            char t_padding[12];
        } __attribute__((packed));

        uint64_t octToDec(const char *str, size_t size) {
            uint64_t n = 0;
            const char *c = str;

            while (size-- > 0) {
                if (*c == '\0' || *c == ' ') {
                    break;
                }

                if (*c >= '0' && *c <= '7') {
                    n *= 8;
                    n += (*c - '0');
                }
                c++;
            }

            return n;
        }

        void int_to_bin_string(uint64_t num, char *buffer, int num_bits) {
            buffer[num_bits] = '\0';

            for (int i = num_bits - 1; i >= 0; i--) {
                buffer[i] = (num & 1) ? '1' : '0';

                num >>= 1;
            }
        }

        int parseHeader(int fd, USTAR_Hdr *out_hdr) {
            char *header_str = static_cast<char *>(Heap::kmalloc(sizeof(USTAR_Hdr)));

            if (!header_str) {
                printf("%rERROR: Could not allocate header buffer%r\n", VGA::RED, VGA::WHITE);
                Heap::kfree(header_str);
                return -1;
            }

            VFS::read(fd, header_str, sizeof(USTAR_Hdr));
            USTAR_Hdr hdr {};
            memmove(&hdr, header_str, sizeof(USTAR_Hdr));
            Heap::kfree(header_str);

            if (hdr.t_magic[0] == 0) {
                return 1;
            }

            memmove(&hdr, out_hdr, sizeof(USTAR_Hdr));
            return 0;
        }
    }

    uint64_t dataSizeWithPadding(uint64_t size) {
        return size + ((512 - (size % 512)) % 512);
    }

    void listFiles(const char *path) {
        int fd = VFS::open(path);
        if (fd < 0) {
            printf("%rCould not open archive file: %s%r\n", VGA::RED, path, VGA::WHITE);
            return;
        }
        auto header = static_cast<char *>(Heap::kmalloc(sizeof(USTAR_Hdr)));
        USTAR_Hdr hdr {};
        while (true) {
            VFS::read(fd, header, 512);
            memcpy(&hdr, header, 512);
            if (hdr.t_magic[0] == 0) break;
            printf("   FILE: %s -- size: %u\n", hdr.t_file_name, octToDec(hdr.t_file_size, 12));
            VFS::read(fd, nullptr, dataSizeWithPadding(octToDec(hdr.t_file_size, 12)));
        }
        Heap::kfree(header);
        VFS::close(fd);
    }


    void catFile(const char *archive, const char *filename) {
        int fd = VFS::open(archive);
        if (fd < 0) {
            printf("%rCould not open archive file: %s%r\n", VGA::RED, archive, VGA::WHITE);
            return;
        }
        auto header = static_cast<char *>(Heap::kmalloc(sizeof(USTAR_Hdr)));
        USTAR_Hdr hdr {};
        uint64_t offset = 0;
        while (strcmp(filename, hdr.t_file_name)) {
            VFS::read(fd, nullptr, offset);
            VFS::read(fd, header, 512);
            memcpy(&hdr, header, 512);
            if (hdr.t_magic[0] == 0) {
                printf("%rFile not in archive%r\n", VGA::RED, VGA::WHITE);
                Heap::kfree(header);
                VFS::close(fd);
                return;
            }
            offset = dataSizeWithPadding(octToDec(hdr.t_file_size, 12));
            Timer::sleepMiliseconds(10);
        }
        uint64_t size = octToDec(hdr.t_file_size, 12);
        char *content = static_cast<char *>(Heap::kmalloc(size + 1));
        if (!content) {
            printf("%rCould not allocate buffer for file%r\n", VGA::RED, VGA::WHITE);
            Heap::kfree(header);
            VFS::close(fd);
            return;
        }
        Timer::sleepMiliseconds(10);
        VFS::read(fd, content, size);
        content[size] = '\0';
        Timer::sleepMiliseconds(10);
        printf("%s\n", content);
        Heap::kfree(header);
        Heap::kfree(content);
        VFS::close(fd);
    }

    void extractFile(const char *archive, const char *filename, const char *target_file) {
        int fd = VFS::open(archive);
        if (fd < 0) {
            printf("%rCould not open archive file: %s%r\n", VGA::RED, archive, VGA::WHITE);
            return;
        }
        auto header = static_cast<char *>(Heap::kmalloc(sizeof(USTAR_Hdr)));
        USTAR_Hdr hdr {};
        uint64_t offset = 0;
        while (strcmp(filename, hdr.t_file_name)) {
            VFS::read(fd, nullptr, offset);
            VFS::read(fd, header, 512);
            memcpy(&hdr, header, 512);
            if (hdr.t_magic[0] == 0) {
                printf("%rFile not in archive%r\n", VGA::RED, VGA::WHITE);
                Heap::kfree(header);
                VFS::close(fd);
                return;
            }
            offset = dataSizeWithPadding(octToDec(hdr.t_file_size, 12));
            Timer::sleepMiliseconds(10);
        }
        uint64_t size = octToDec(hdr.t_file_size, 12);
        char *content = static_cast<char *>(Heap::kmalloc(size + 1));
        if (!content) {
            printf("%rCould not allocate buffer for file%r\n", VGA::RED, VGA::WHITE);
            Heap::kfree(header);
            VFS::close(fd);
            return;
        }
        Timer::sleepMiliseconds(10);
        VFS::read(fd, content, size);
        content[size] = '\0';
        Timer::sleepMiliseconds(10);
        if (!VFS::create(target_file, false)) {
            printf("%rERROR: could not create output file%r\n", VGA::RED, VGA::WHITE);
            VFS::close(fd);
            Heap::kfree(header);
            Heap::kfree(content);
            return;
        }
        int out_fd = VFS::open(target_file);
        VFS::write(out_fd, content, size);
        Heap::kfree(header);
        Heap::kfree(content);
        VFS::close(fd);
        VFS::close(out_fd);
    }
}
