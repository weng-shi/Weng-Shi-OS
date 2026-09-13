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
#include <stddef.h>

namespace VFS {


    using ReadSectorsFn = bool (*)(int device_handle, uint64_t lba, uint32_t count, void *out_buf);
    using WriteSectorsFn = bool (*)(int device_handle, uint64_t lba, uint32_t count, const void *in_buf);

    struct BlockDevice {
        char name[32];
        int device_handle;
        uint64_t sector_count;
        uint32_t sector_size;
        ReadSectorsFn read;
        WriteSectorsFn write;
    };

    constexpr int MAX_BLOCK_DEVICES = 16;

    bool registerBlockDevice(const char *name, int device_handle, uint64_t sector_count,
                             uint32_t sector_size, ReadSectorsFn read, WriteSectorsFn write);

    int blockDeviceCount();

    const BlockDevice *getBlockDevice(int index);

    const BlockDevice *findBlockDevice(const char *name);



    struct FileHandle {
        void *fs_private;
        uint64_t size;
        bool is_directory;
    };

    struct DirEntry {
        char name[64];
        bool is_directory;
    };

    struct FilesystemDriver {
        const char *name;

        void * (*mount)(BlockDevice *dev);

        void (*unmount)(void *fs_instance);

        bool (*open)(void *fs_instance, const char *path, FileHandle *out_handle);

        void (*close)(void *fs_instance, FileHandle *handle);

        int64_t (*read)(void *fs_instance, FileHandle *handle, uint64_t offset, void *buf, size_t count);

        int64_t (*write)(void *fs_instance, FileHandle *handle, uint64_t offset, const void *buf, size_t count);

        bool (*isDirectory)(void *fs_instance, const char *path);

        bool (*readdir)(void *fs_instance, const char *path, int index, DirEntry *out_entry);

        bool (*create)(void *fs_instance, const char *path, bool is_directory);

        bool (*unlinkFile)(void* fs_instance, const char* path);

        bool (*rmdir)(void* fs_instance, const char* path);
    };

    constexpr int MAX_MOUNTS = 8;

    bool mount(const char *mount_point, const char *device_name, FilesystemDriver *driver);

    bool unmount(const char *mount_point);

    struct MountInfo {
        char mount_point[64];
        char device_name[32];
        const char *fs_name;
    };

    int mountCount();

    const MountInfo *getMount(int index);

    constexpr int MAX_OPEN_FILES = 32;
    constexpr int INVALID_FD = -1;

    int open(const char *path);
    int64_t read(int fd, void *buf, size_t count);

    int64_t write(int fd, const void *buf, size_t count);

    void close(int fd);

    bool create(const char *path, bool is_directory);

    void normalizePath(const char *cwd, const char *input, char *out, size_t out_size);

    bool isDirectory(const char *path);

    bool listDirectory(const char *path, int index, DirEntry *out_entry);

    bool unlink(const char* path);

    bool rmdir(const char* path);

    enum class SeekMode { Set, Cur, End };
    int64_t seek(int fd, int64_t offset, SeekMode mode);

    int64_t tell(int fd);

    void init();
}
