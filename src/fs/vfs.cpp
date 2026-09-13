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

#include <vfs.h>
#include <string.h>
#include <terminal.h>

namespace VFS {
    namespace {
        BlockDevice g_block_devices[MAX_BLOCK_DEVICES];
        int g_block_device_count = 0;

        struct Mount {
            bool in_use;
            char mount_point[64];
            char device_name[32];
            BlockDevice *device;
            FilesystemDriver *driver;
            void *fs_instance;
        };

        Mount g_mounts[MAX_MOUNTS];

        struct OpenFile {
            bool in_use;
            Mount *mount;
            FileHandle handle;
            uint64_t offset;
        };

        OpenFile g_open_files[MAX_OPEN_FILES];

        Mount *resolvePath(const char *path, char *out_subpath, size_t out_subpath_size) {
            Mount *best = nullptr;
            size_t best_len = 0;

            for (int i = 0; i < MAX_MOUNTS; ++i) {
                if (!g_mounts[i].in_use) continue;

                size_t mp_len = strlen(g_mounts[i].mount_point);
                if (strncmp(path, g_mounts[i].mount_point, mp_len) == 0) {
                    char next = path[mp_len];
                    if (next == '\0' || next == '/') {
                        if (mp_len > best_len) {
                            best = &g_mounts[i];
                            best_len = mp_len;
                        }
                    }
                }
            }

            if (!best) return nullptr;

            const char *remainder = path + best_len;
            if (*remainder == '/') ++remainder;

            size_t i = 0;
            while (remainder[i] != '\0' && i < out_subpath_size - 1) {
                out_subpath[i] = remainder[i];
                ++i;
            }
            out_subpath[i] = '\0';

            return best;
        }

        bool pathEquals(const char *a, const char *b) {
            return strcmp(a, b) == 0;
        }

        bool startsWith(const char *str, const char *prefix) {
            size_t len = strlen(prefix);
            return strncmp(str, prefix, len) == 0;
        }
    }

    void init() {
        g_block_device_count = 0;
        for (int i = 0; i < MAX_MOUNTS; ++i) g_mounts[i].in_use = false;
        for (int i = 0; i < MAX_OPEN_FILES; ++i) g_open_files[i].in_use = false;
    }


    bool registerBlockDevice(const char *name, int device_handle, uint64_t sector_count,
                             uint32_t sector_size, ReadSectorsFn read, WriteSectorsFn write) {
        if (g_block_device_count >= MAX_BLOCK_DEVICES) return false;

        BlockDevice &dev = g_block_devices[g_block_device_count];
        strncpy(dev.name, name, sizeof(dev.name) - 1);
        dev.name[sizeof(dev.name) - 1] = '\0';
        dev.device_handle = device_handle;
        dev.sector_count = sector_count;
        dev.sector_size = sector_size;
        dev.read = read;
        dev.write = write;

        ++g_block_device_count;
        return true;
    }

    int blockDeviceCount() { return g_block_device_count; }

    const BlockDevice *getBlockDevice(int index) {
        if (index < 0 || index >= g_block_device_count) return nullptr;
        return &g_block_devices[index];
    }

    const BlockDevice *findBlockDevice(const char *name) {
        for (int i = 0; i < g_block_device_count; ++i) {
            if (strcmp(g_block_devices[i].name, name) == 0) return &g_block_devices[i];
        }
        return nullptr;
    }


    bool mount(const char *mount_point, const char *device_name, FilesystemDriver *driver) {
        BlockDevice *dev = const_cast<BlockDevice *>(findBlockDevice(device_name));
        if (!dev) return false;

        for (int i = 0; i < MAX_MOUNTS; ++i) {
            if (g_mounts[i].in_use && strcmp(g_mounts[i].mount_point, mount_point) == 0) {
                return false;
            }
        }

        for (int i = 0; i < MAX_MOUNTS; ++i) {
            if (!g_mounts[i].in_use) {
                void *instance = driver->mount(dev);
                if (!instance) return false;

                g_mounts[i].in_use = true;
                strncpy(g_mounts[i].mount_point, mount_point, sizeof(g_mounts[i].mount_point) - 1);
                g_mounts[i].mount_point[sizeof(g_mounts[i].mount_point) - 1] = '\0';
                strncpy(g_mounts[i].device_name, device_name, sizeof(g_mounts[i].device_name) - 1);
                g_mounts[i].device_name[sizeof(g_mounts[i].device_name) - 1] = '\0';
                g_mounts[i].device = dev;
                g_mounts[i].driver = driver;
                g_mounts[i].fs_instance = instance;
                return true;
            }
        }
        return false;
    }

    bool unmount(const char *mount_point) {
        for (int i = 0; i < MAX_MOUNTS; ++i) {
            if (g_mounts[i].in_use && strcmp(g_mounts[i].mount_point, mount_point) == 0) {
                g_mounts[i].driver->unmount(g_mounts[i].fs_instance);
                g_mounts[i].in_use = false;
                return true;
            }
        }
        return false;
    }

    int mountCount() {
        int count = 0;
        for (int i = 0; i < MAX_MOUNTS; ++i) if (g_mounts[i].in_use) ++count;
        return count;
    }

    const MountInfo *getMount(int index) {
        static MountInfo info;
        int seen = 0;
        for (int i = 0; i < MAX_MOUNTS; ++i) {
            if (!g_mounts[i].in_use) continue;
            if (seen == index) {
                strncpy(info.mount_point, g_mounts[i].mount_point, sizeof(info.mount_point) - 1);
                info.mount_point[sizeof(info.mount_point) - 1] = '\0';
                strncpy(info.device_name, g_mounts[i].device_name, sizeof(info.device_name) - 1);
                info.device_name[sizeof(info.device_name) - 1] = '\0';
                info.fs_name = g_mounts[i].driver->name;
                return &info;
            }
            ++seen;
        }
        return nullptr;
    }


    int open(const char *path) {
        char subpath[128];
        Mount *mnt = resolvePath(path, subpath, sizeof(subpath));
        if (!mnt) return INVALID_FD;

        for (int i = 0; i < MAX_OPEN_FILES; ++i) {
            if (!g_open_files[i].in_use) {
                FileHandle handle{};
                if (!mnt->driver->open(mnt->fs_instance, subpath, &handle)) {
                    return INVALID_FD;
                }
                g_open_files[i].in_use = true;
                g_open_files[i].mount = mnt;
                g_open_files[i].handle = handle;
                g_open_files[i].offset = 0;
                return i;
            }
        }
        return INVALID_FD;
    }

    int64_t read(int fd, void *buf, size_t count) {
        if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].in_use) return -1;

        OpenFile &f = g_open_files[fd];
        int64_t n = f.mount->driver->read(f.mount->fs_instance, &f.handle, f.offset, buf, count);
        if (n > 0) f.offset += n;
        return n;
    }

    int64_t write(int fd, const void *buf, size_t count) {
        if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].in_use) return -1;

        OpenFile &f = g_open_files[fd];
        int64_t n = f.mount->driver->write(f.mount->fs_instance, &f.handle, f.offset, buf, count);
        if (n > 0) f.offset += n;
        return n;
    }

    void close(int fd) {
        if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].in_use) return;

        OpenFile &f = g_open_files[fd];
        f.mount->driver->close(f.mount->fs_instance, &f.handle);
        f.in_use = false;
    }

    bool create(const char *path, bool is_directory) {
        char subpath[128];
        Mount *mnt = resolvePath(path, subpath, sizeof(subpath));
        if (!mnt) return false;
        return mnt->driver->create(mnt->fs_instance, subpath, is_directory);
    }

    void normalizePath(const char *cwd, const char *input, char *out, size_t out_size) {
        char combined[256];
        size_t ci = 0;

        if (input[0] == '/') {
            combined[ci++] = '/';
        } else {
            for (size_t i = 0; cwd[i] != '\0' && ci < sizeof(combined) - 2; ++i) combined[ci++] = cwd[i];
            if (ci == 0 || combined[ci - 1] != '/') combined[ci++] = '/';
            for (size_t i = 0; input[i] != '\0' && ci < sizeof(combined) - 1; ++i) combined[ci++] = input[i];
        }
        if (input[0] == '/') {
            for (size_t i = 1; input[i] != '\0' && ci < sizeof(combined) - 1; ++i) combined[ci++] = input[i];
        }
        combined[ci] = '\0';

        const char *segments[32];
        int seg_count = 0;

        char *p = combined;
        while (*p == '/') ++p;

        while (*p != '\0') {
            char *start = p;
            while (*p != '\0' && *p != '/') ++p;
            size_t len = p - start;
            while (*p == '/') ++p;

            if (len == 0) continue;
            if (len == 1 && start[0] == '.') continue;
            if (len == 2 && start[0] == '.' && start[1] == '.') {
                if (seg_count > 0) --seg_count;
                continue;
            }
            if (seg_count < 32) {
                static char storage[32][64];
                size_t copy_len = len < 63 ? len : 63;
                for (size_t i = 0; i < copy_len; ++i) storage[seg_count][i] = start[i];
                storage[seg_count][copy_len] = '\0';
                segments[seg_count] = storage[seg_count];
                ++seg_count;
            }
        }

        size_t oi = 0;
        out[oi++] = '/';
        for (int i = 0; i < seg_count; ++i) {
            for (size_t j = 0; segments[i][j] != '\0' && oi < out_size - 2; ++j) out[oi++] = segments[i][j];
            if (i != seg_count - 1) out[oi++] = '/';
        }
        out[oi] = '\0';
    }

    bool isDirectory(const char *path) {
        if (pathEquals(path, "/")) return true;
        if (pathEquals(path, "/dev")) return true;
        if (pathEquals(path, "/dev/disks")) return true;

        for (int i = 0; i < MAX_MOUNTS; ++i) {
            if (g_mounts[i].in_use && pathEquals(g_mounts[i].mount_point, path)) return true;
        }

        char subpath[128];
        Mount *mnt = resolvePath(path, subpath, sizeof(subpath));
        if (!mnt) return false;

        return mnt->driver->isDirectory(mnt->fs_instance, subpath);
    }

    bool listDirectory(const char *path, int index, DirEntry *out_entry) {
        if (pathEquals(path, "/")) {
            static const char *fixed[] = {"dev"};
            int fixed_count = 1;

            if (index < fixed_count) {
                strncpy(out_entry->name, fixed[index], sizeof(out_entry->name) - 1);
                out_entry->name[sizeof(out_entry->name) - 1] = '\0';
                out_entry->is_directory = true;
                return true;
            }

            int seen = 0;
            char shown[MAX_MOUNTS][32];
            int shown_count = 0;

            for (int i = 0; i < MAX_MOUNTS; ++i) {
                if (!g_mounts[i].in_use) continue;
                const char *mp = g_mounts[i].mount_point;
                if (mp[0] != '/' || mp[1] == '\0') continue;

                const char *start = mp + 1;
                const char *end = start;
                while (*end != '\0' && *end != '/') ++end;
                size_t len = end - start;

                bool dup = false;
                for (int s = 0; s < shown_count; ++s) {
                    if (strncmp(shown[s], start, len) == 0 && shown[s][len] == '\0') {
                        dup = true;
                        break;
                    }
                }
                if (dup) continue;

                for (size_t c = 0; c < len && c < 31; ++c) shown[shown_count][c] = start[c];
                shown[shown_count][len < 31 ? len : 31] = '\0';
                ++shown_count;

                if (seen == index - fixed_count) {
                    strncpy(out_entry->name, shown[shown_count - 1], sizeof(out_entry->name) - 1);
                    out_entry->name[sizeof(out_entry->name) - 1] = '\0';
                    out_entry->is_directory = true;
                    return true;
                }
                ++seen;
            }
            return false;
        }

        if (pathEquals(path, "/dev")) {
            if (index == 0) {
                strncpy(out_entry->name, "disks", sizeof(out_entry->name) - 1);
                out_entry->is_directory = true;
                return true;
            }
            return false;
        }

        if (pathEquals(path, "/dev/disks")) {
            if (index < 0 || index >= g_block_device_count) return false;
            strncpy(out_entry->name, g_block_devices[index].name, sizeof(out_entry->name) - 1);
            out_entry->name[sizeof(out_entry->name) - 1] = '\0';
            out_entry->is_directory = false;
            return true;
        }

        char subpath[128];
        Mount *mnt = resolvePath(path, subpath, sizeof(subpath));
        if (!mnt) return false;

        return mnt->driver->readdir(mnt->fs_instance, subpath, index, out_entry);
    }

    bool unlink(const char* path) {
        char subpath[128];
        Mount* mnt = resolvePath(path, subpath, sizeof(subpath));
        if (!mnt) return false;
        return mnt->driver->unlinkFile(mnt->fs_instance, subpath);
    }

    bool rmdir(const char* path) {
        char subpath[128];
        Mount* mnt = resolvePath(path, subpath, sizeof(subpath));
        if (!mnt) return false;
        return mnt->driver->rmdir(mnt->fs_instance, subpath);
    }

    int64_t seek(int fd, int64_t offset, SeekMode mode) {
        if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].in_use) return -1;

        OpenFile& f = g_open_files[fd];
        int64_t new_offset;

        switch (mode) {
            case SeekMode::Set:
                new_offset = offset;
                break;
            case SeekMode::Cur:
                new_offset = static_cast<int64_t>(f.offset) + offset;
                break;
            case SeekMode::End:
                new_offset = static_cast<int64_t>(f.handle.size) + offset;
                break;
            default:
                return -1;
        }

        if (new_offset < 0) return -1;

        f.offset = static_cast<uint64_t>(new_offset);
        return new_offset;
    }

    int64_t tell(int fd) {
        if (fd < 0 || fd >= MAX_OPEN_FILES || !g_open_files[fd].in_use) return -1;
        return static_cast<int64_t>(g_open_files[fd].offset);
    }
}
