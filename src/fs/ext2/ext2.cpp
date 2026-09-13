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

#include <ext2.h>
#include <heap.h>
#include <string.h>
#include <terminal.h>

namespace Ext2 {
    namespace {
        constexpr uint16_t EXT2_MAGIC = 0xEF53;
        constexpr uint32_t ROOT_INODE = 2;

        constexpr uint16_t S_IFDIR = 0x4000;
        constexpr uint16_t S_IFREG = 0x8000;
        constexpr uint16_t S_IFMT = 0xF000;

#pragma pack(push, 1)
        struct Superblock {
            uint32_t inodes_count;
            uint32_t blocks_count;
            uint32_t r_blocks_count;
            uint32_t free_blocks_count;
            uint32_t free_inodes_count;
            uint32_t first_data_block;
            uint32_t log_block_size;
            uint32_t log_frag_size;
            uint32_t block_per_group;
            uint32_t frags_per_group;
            uint32_t inodes_per_group;
            uint32_t mtime;
            uint32_t wtime;
            uint16_t mnt_count;
            uint16_t max_mnt_count;
            uint16_t magic;
            uint16_t state;
            uint16_t errors;
            uint16_t minor_rev_level;
            uint32_t lastcheck;
            uint32_t checkinterval;
            uint32_t creator_os;
            uint32_t rev_level;
            uint16_t def_resuid;
            uint16_t def_resgid;

            uint32_t first_ino;
            uint16_t inode_size;
            uint16_t block_group_nr;
            uint32_t feature_compact;
            uint32_t feature_incompact;
            uint32_t feature_ro_compact;
            uint8_t uuid[16];
            char volume_name[16];
            char last_mounted[64];
            uint32_t algo_bitmap;
            uint8_t reserved[1024 - 204];
        };

        struct GroupDesc {
            uint32_t block_bitmap;
            uint32_t inode_bitmap;
            uint32_t inode_table;
            uint16_t free_blocks_count;
            uint16_t free_inodes_count;
            uint16_t used_dirs_count;
            uint16_t pad;
            uint8_t reserved[12];
        };

        struct Inode {
            uint16_t mode;
            uint16_t uid;
            uint32_t size;
            uint32_t atime;
            uint32_t ctime;
            uint32_t mtime;
            uint32_t dtime;
            uint16_t gid;
            uint16_t links_count;
            uint32_t blocks;
            uint32_t flags;
            uint32_t osd1;
            uint32_t block[15];
            uint32_t generation;
            uint32_t file_acl;
            uint32_t dir_acl;
            uint32_t faddr;
            uint8_t osd2[12];
        };

        struct DirEntryOnDisk {
            uint32_t inode;
            uint16_t rec_len;
            uint8_t name_len;
            uint8_t file_type;
        };

#pragma pack(pop)
        struct FsInstance {
            VFS::BlockDevice *dev;
            Superblock sb;
            GroupDesc *groups;
            uint32_t group_count;
            uint32_t block_size;
        };

        bool readBlock(FsInstance *fs, uint32_t block_num, void *out_buf) {
            uint32_t sectors_per_block = fs->block_size / fs->dev->sector_size;
            uint64_t lba = static_cast<uint64_t>(block_num) * sectors_per_block;
            return fs->dev->read(fs->dev->device_handle, lba, sectors_per_block, out_buf);
        }

        bool writeBlock(FsInstance *fs, uint32_t block_num, const void *buf);

        bool writeSuperblock(FsInstance *fs);

        bool writeGroupDescriptors(FsInstance *fs);

        uint32_t inodeBlockAtOrAlloc(FsInstance *fs, Inode *inode, uint32_t index);

        uint32_t inodeBlockAt(FsInstance *fs, Inode *inode, uint32_t index);


        bool readInode(FsInstance *fs, uint32_t inode_num, Inode *out_inode) {
            if (inode_num == 0) return false;

            uint32_t group = (inode_num - 1) / fs->sb.inodes_per_group;
            uint32_t index_in_group = (inode_num - 1) % fs->sb.inodes_per_group;

            if (group >= fs->group_count) return false;

            uint32_t inode_size = fs->sb.inode_size ? fs->sb.inode_size : 128;
            uint32_t byte_offset_in_table = index_in_group * inode_size;
            uint32_t block_offset = byte_offset_in_table / fs->block_size;
            uint32_t offset_in_block = byte_offset_in_table % fs->block_size;

            uint32_t block_num = fs->groups[group].inode_table + block_offset;

            uint8_t *buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!buf) return false;

            bool ok = readBlock(fs, block_num, buf);
            if (ok) {
                memcpy(out_inode, buf + offset_in_block, sizeof(Inode) < inode_size ? sizeof(Inode) : inode_size);
            }

            Heap::kfree(buf);
            return ok;
        }

        bool writeInode(FsInstance *fs, uint32_t inode_num, Inode *inode) {
            if (inode_num == 0) return false;

            uint32_t group = (inode_num - 1) / fs->sb.inodes_per_group;
            uint32_t index_in_group = (inode_num - 1) % fs->sb.inodes_per_group;
            if (group >= fs->group_count) return false;

            uint32_t inode_size = fs->sb.inode_size ? fs->sb.inode_size : 128;
            uint32_t byte_offset_in_table = index_in_group * inode_size;
            uint32_t block_offset = byte_offset_in_table / fs->block_size;
            uint32_t offset_in_block = byte_offset_in_table % fs->block_size;

            uint32_t block_num = fs->groups[group].inode_table + block_offset;

            uint8_t *buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!buf) return false;

            bool ok = readBlock(fs, block_num, buf);
            if (ok) {
                memcpy(buf + offset_in_block, inode, sizeof(Inode));
                ok = writeBlock(fs, block_num, buf);
            }

            Heap::kfree(buf);
            return ok;
        }

        bool freeInode(FsInstance *fs, uint32_t inode_num) {
            if (inode_num == 0) return false;

            uint32_t group = (inode_num - 1) / fs->sb.inodes_per_group;
            uint32_t index_in_group = (inode_num - 1) % fs->sb.inodes_per_group;
            if (group >= fs->group_count) return false;

            uint8_t *bitmap = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!bitmap) return false;

            if (!readBlock(fs, fs->groups[group].inode_bitmap, bitmap)) {
                Heap::kfree(bitmap);
                return false;
            }

            uint32_t byte_idx = index_in_group / 8;
            uint8_t bit_idx = index_in_group % 8;
            bitmap[byte_idx] &= ~(1 << bit_idx);

            bool ok = writeBlock(fs, fs->groups[group].inode_bitmap, bitmap);
            Heap::kfree(bitmap);
            if (!ok) return false;

            fs->groups[group].free_inodes_count++;
            fs->sb.free_inodes_count++;
            writeGroupDescriptors(fs);
            writeSuperblock(fs);
            return true;
        }

        uint32_t lookupInDir(FsInstance *fs, Inode *dir_inode, const char *name) {
            if ((dir_inode->mode & S_IFMT) != S_IFDIR) return 0;

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return 0;

            size_t name_len = strlen(name);
            uint32_t result = 0;

            uint32_t num_blocks = (dir_inode->size + fs->block_size - 1) / fs->block_size;

            for (uint32_t b = 0; b < num_blocks && result == 0; ++b) {
                uint32_t phys_block = inodeBlockAt(fs, dir_inode, b);
                if (phys_block == 0) continue;
                if (!readBlock(fs, phys_block, block_buf)) continue;

                uint32_t offset = 0;

                while (offset < fs->block_size) {
                    auto *entry = reinterpret_cast<DirEntryOnDisk *>(block_buf + offset);
                    if (entry->rec_len == 0) break;

                    if (entry->inode != 0 && entry->name_len == name_len) {
                        const char *entry_name = reinterpret_cast<const char *>(entry) + sizeof(DirEntryOnDisk);
                        if (strncmp(entry_name, name, name_len) == 0) {
                            result = entry->inode;
                            break;
                        }
                    }

                    offset += entry->rec_len;
                }
            }
            Heap::kfree(block_buf);
            return result;
        }

        uint32_t resolvePathToInode(FsInstance *fs, const char *path) {
            if (path[0] == '\0') return ROOT_INODE;

            uint32_t current = ROOT_INODE;

            char component[64];
            size_t ci = 0;
            const char *p = path;

            while (true) {
                if (*p == '/' || *p == '\0') {
                    if (ci > 0) {
                        component[ci] = '\0';

                        Inode dir_inode;

                        if (!readInode(fs, current, &dir_inode)) return 0;

                        current = lookupInDir(fs, &dir_inode, component);
                        if (current == 0) return 0;

                        ci = 0;
                    }
                    if (*p == '\0') break;
                    ++p;
                } else {
                    if (ci < sizeof(component) - 1) component[ci++] = *p;
                    ++p;
                }
            }
            return current;
        }

        void *mount(VFS::BlockDevice *dev) {
            uint8_t sb_buf[1024];

            if (!dev->read(dev->device_handle, 2, 1024 / dev->sector_size, sb_buf)) {
                return nullptr;
            }

            auto *sb = reinterpret_cast<Superblock *>(sb_buf);
            if (sb->magic != EXT2_MAGIC) {
                g_terminal.write("ext2: bad magic, not an ext2 filsesystem\n");
                return nullptr;
            }

            auto *fs = static_cast<FsInstance *>(Heap::kmalloc(sizeof(FsInstance)));
            if (!fs) return nullptr;

            memcpy(&fs->sb, sb_buf, sizeof(Superblock));
            fs->dev = dev;
            fs->block_size = 1024 << fs->sb.log_block_size;

            if (fs->sb.rev_level == 0) {
                fs->sb.inode_size = 128;
                fs->sb.first_ino = 11;
            }

            fs->group_count = (fs->sb.blocks_count + fs->sb.block_per_group - 1) / fs->sb.block_per_group;
            fs->groups = static_cast<GroupDesc *>(Heap::kmalloc(sizeof(GroupDesc) * fs->group_count));

            if (!fs->groups) {
                Heap::kfree(fs);
                return nullptr;
            }


            uint32_t bgdt_block = fs->sb.first_data_block + 1;
            uint32_t bgdt_bytes = sizeof(GroupDesc) * fs->group_count;
            uint32_t bgdt_blocks_needed = (bgdt_bytes + fs->block_size - 1) / fs->block_size;

            uint8_t *bgdt_buf = static_cast<uint8_t *>(Heap::kmalloc(bgdt_blocks_needed * fs->block_size));
            if (!bgdt_buf) {
                Heap::kfree(fs->groups);
                Heap::kfree(fs);
                return nullptr;
            }

            for (uint32_t i = 0; i < bgdt_blocks_needed; ++i) {
                if (!readBlock(fs, bgdt_block + i, bgdt_buf + i * fs->block_size)) {
                    Heap::kfree(bgdt_buf);
                    Heap::kfree(fs->groups);
                    Heap::kfree(fs);
                    return nullptr;
                }
            }
            memcpy(fs->groups, bgdt_buf, bgdt_bytes);
            Heap::kfree(bgdt_buf);

            g_terminal.write("ext2: mounted, block_size=");
            g_terminal.writeDec(fs->block_size);
            g_terminal.write(", groups=");
            g_terminal.writeDec(fs->group_count);
            g_terminal.write("\n");

            return fs;
        }

        void unmount(void *fs_instance) {
            auto *fs = static_cast<FsInstance *>(fs_instance);
            Heap::kfree(fs->groups);
            Heap::kfree(fs);
        }

        bool open(void *fs_instance, const char *path, VFS::FileHandle *out_handle) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            uint32_t inode_num = resolvePathToInode(fs, path);
            if (inode_num == 0) return false;

            auto *inode = static_cast<Inode *>(Heap::kmalloc(sizeof(Inode)));
            if (!inode) return false;

            if (!readInode(fs, inode_num, inode)) {
                Heap::kfree(inode);
                return false;
            }

            struct PrivateData {
                uint32_t inode_num;
                Inode inode;
            };

            auto *priv = static_cast<PrivateData *>(Heap::kmalloc(sizeof(PrivateData)));
            if (!priv) {
                Heap::kfree(inode);
                return false;
            }

            priv->inode_num = inode_num;
            priv->inode = *inode;
            Heap::kfree(inode);

            out_handle->fs_private = priv;
            out_handle->size = priv->inode.size;
            out_handle->is_directory = (priv->inode.mode & S_IFMT) == S_IFDIR;

            return true;
        }

        void close(void *, VFS::FileHandle *handle) {
            if (handle->fs_private) {
                Heap::kfree(handle->fs_private);
                handle->fs_private = nullptr;
            }
        }

        int64_t read(void *fs_instance, VFS::FileHandle *handle, uint64_t offset, void *buf, size_t count) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            struct PrivateData {
                uint32_t inode_num;
                Inode inode;
            };

            auto *priv = static_cast<PrivateData *>(handle->fs_private);

            if (offset >= priv->inode.size) return 0;
            if (offset + count > priv->inode.size) count = priv->inode.size - offset;

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return -1;

            uint8_t *dst = static_cast<uint8_t *>(buf);
            size_t bytes_read = 0;

            while (bytes_read < count) {
                uint64_t file_pos = offset + bytes_read;
                uint32_t logical_block = static_cast<uint32_t>(file_pos / fs->block_size);
                uint32_t offset_in_block = static_cast<uint32_t>(file_pos % fs->block_size);

                uint32_t phys_block = inodeBlockAtOrAlloc(fs, &priv->inode, logical_block);

                size_t chunk = fs->block_size - offset_in_block;
                if (chunk > count - bytes_read) chunk = count - bytes_read;

                if (phys_block == 0) {
                    memset(dst + bytes_read, 0, chunk);
                } else {
                    if (!readBlock(fs, phys_block, block_buf)) {
                        Heap::kfree(block_buf);
                        return bytes_read > 0 ? static_cast<int64_t>(bytes_read) : -1;
                    }
                    memcpy(dst + bytes_read, block_buf + offset_in_block, chunk);
                }
                bytes_read += chunk;
            }

            Heap::kfree(block_buf);
            return static_cast<int64_t>(bytes_read);
        }

        int64_t write(void *fs_instance, VFS::FileHandle *handle, uint64_t offset,
                      const void *buf, size_t count) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            struct PrivateData {
                uint32_t inode_num;
                Inode inode;
            };
            auto *priv = static_cast<PrivateData *>(handle->fs_private);

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return -1;

            const uint8_t *src = static_cast<const uint8_t *>(buf);
            size_t bytes_written = 0;

            while (bytes_written < count) {
                uint64_t file_pos = offset + bytes_written;
                uint32_t logical_block = static_cast<uint32_t>(file_pos / fs->block_size);
                uint32_t offset_in_block = static_cast<uint32_t>(file_pos % fs->block_size);

                uint32_t phys_block = inodeBlockAtOrAlloc(fs, &priv->inode, logical_block);
                if (phys_block == 0) break;

                size_t chunk = fs->block_size - offset_in_block;
                if (chunk > count - bytes_written) chunk = count - bytes_written;

                if (offset_in_block != 0 || chunk != fs->block_size) {
                    if (!readBlock(fs, phys_block, block_buf)) break;
                }
                memcpy(block_buf + offset_in_block, src + bytes_written, chunk);
                if (!writeBlock(fs, phys_block, block_buf)) break;

                bytes_written += chunk;
            }

            Heap::kfree(block_buf);

            uint64_t new_size = offset + bytes_written;
            if (new_size > priv->inode.size) {
                priv->inode.size = static_cast<uint32_t>(new_size);
                handle->size = priv->inode.size;
            }
            writeInode(fs, priv->inode_num, &priv->inode);

            return static_cast<int64_t>(bytes_written);
        }


        bool isDirectory(void *fs_instance, const char *path) {
            auto *fs = static_cast<FsInstance *>(fs_instance);
            uint32_t inode_num = resolvePathToInode(fs, path);
            if (inode_num == 0) return false;

            Inode inode;
            if (!readInode(fs, inode_num, &inode)) return false;

            return (inode.mode & S_IFMT) == S_IFDIR;
        }

        constexpr uint8_t FT_UNKNOWN = 0;
        constexpr uint8_t FT_REG_FILE = 1;
        constexpr uint8_t FT_DIR = 2;

        uint32_t dirEntryMinSize(uint8_t name_len) {
            uint32_t base = sizeof(DirEntryOnDisk) + name_len;
            return (base + 3) & ~3u;
        }

        bool insertDirEntry(FsInstance *fs, Inode *dir_inode, const char *name,
                            uint32_t child_inode, uint8_t file_type) {
            uint8_t name_len = static_cast<uint8_t>(strlen(name));
            uint32_t needed = dirEntryMinSize(name_len);

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return false;

            uint32_t num_blocks = (dir_inode->size + fs->block_size - 1) / fs->block_size;

            for (uint32_t b = 0; b < num_blocks; ++b) {
                uint32_t phys_block = inodeBlockAt(fs, dir_inode, b);
                if (phys_block == 0) continue;
                if (!readBlock(fs, phys_block, block_buf)) continue;

                uint32_t offset = 0;
                while (offset < fs->block_size) {
                    auto *entry = reinterpret_cast<DirEntryOnDisk *>(block_buf + offset);

                    if (entry->rec_len == 0) break;

                    uint32_t actual_used = (entry->inode == 0) ? 0 : dirEntryMinSize(entry->name_len);
                    uint32_t slack = entry->rec_len - actual_used;

                    if (slack >= needed) {
                        if (entry->inode == 0) {
                            entry->inode = child_inode;
                            entry->name_len = name_len;
                            entry->file_type = file_type;
                            char *name_dst = reinterpret_cast<char *>(entry) + sizeof(DirEntryOnDisk);
                            memcpy(name_dst, name, name_len);
                        } else {
                            uint32_t old_rec_len = entry->rec_len;
                            entry->rec_len = static_cast<uint16_t>(actual_used);

                            auto *new_entry = reinterpret_cast<DirEntryOnDisk *>(block_buf + offset + actual_used);
                            new_entry->inode = child_inode;
                            new_entry->rec_len = static_cast<uint16_t>(old_rec_len - actual_used);
                            new_entry->name_len = name_len;
                            new_entry->file_type = file_type;
                            char *name_dst = reinterpret_cast<char *>(new_entry) + sizeof(DirEntryOnDisk);
                            memcpy(name_dst, name, name_len);
                        }

                        bool ok = writeBlock(fs, phys_block, block_buf);
                        Heap::kfree(block_buf);
                        return ok;
                    }

                    offset += entry->rec_len;
                }
            }

            uint32_t new_block_index = num_blocks;
            uint32_t phys_block = inodeBlockAtOrAlloc(fs, dir_inode, new_block_index);
            if (phys_block == 0) {
                Heap::kfree(block_buf);
                return false;
            }

            for (uint32_t i = 0; i < fs->block_size; ++i) block_buf[i] = 0;

            auto *entry = reinterpret_cast<DirEntryOnDisk *>(block_buf);
            entry->inode = child_inode;
            entry->rec_len = static_cast<uint16_t>(fs->block_size);
            entry->name_len = name_len;
            entry->file_type = file_type;
            memcpy(block_buf + sizeof(DirEntryOnDisk), name, name_len);

            bool ok = writeBlock(fs, phys_block, block_buf);
            Heap::kfree(block_buf);

            if (ok) {
                dir_inode->size += fs->block_size;
            }
            return ok;
        }

        bool readdir(void *fs_instance, const char *path, int index, VFS::DirEntry *out_entry) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            uint32_t dir_inode_num = resolvePathToInode(fs, path);
            if (dir_inode_num == 0) return false;

            Inode dir_inode;
            if (!readInode(fs, dir_inode_num, &dir_inode)) return false;
            if ((dir_inode.mode & S_IFMT) != S_IFDIR) return false;

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return false;

            uint32_t num_blocks = (dir_inode.size + fs->block_size - 1) / fs->block_size;
            int seen = 0;
            bool found = false;

            for (uint32_t b = 0; b < num_blocks && !found; ++b) {
                uint32_t phys_block = inodeBlockAt(fs, &dir_inode, b);

                if (phys_block == 0) continue;
                if (!readBlock(fs, phys_block, block_buf)) continue;

                uint32_t offset = 0;
                while (offset < fs->block_size) {
                    auto *entry = reinterpret_cast<DirEntryOnDisk *>(block_buf + offset);
                    if (entry->rec_len == 0) break;

                    if (entry->inode != 0) {
                        const char *entry_name = reinterpret_cast<const char *>(entry) + sizeof(DirEntryOnDisk);
                        bool is_dot = entry->name_len == 1 && entry_name[0] == '.';
                        bool is_dotdot = entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.';

                        if (!is_dot && !is_dotdot) {
                            if (seen == index) {
                                size_t copy_len = entry->name_len < 63 ? entry->name_len : 63;
                                memcpy(out_entry->name, entry_name, copy_len);
                                out_entry->name[copy_len] = '\0';

                                out_entry->is_directory = (entry->file_type == FT_DIR);
                                found = true;
                                break;
                            }
                            ++seen;
                        }
                    }
                    offset += entry->rec_len;
                }
            }
            Heap::kfree(block_buf);
            return found;
        }

        uint32_t allocBlockInGroup(FsInstance *fs, uint32_t group) {
            if (group >= fs->group_count) return 0;
            if (fs->groups[group].free_blocks_count == 0) return 0;

            uint8_t *bitmap = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!bitmap) return 0;

            if (!readBlock(fs, fs->groups[group].block_bitmap, bitmap)) {
                Heap::kfree(bitmap);
                return 0;
            }

            uint32_t blocks_in_group = fs->sb.block_per_group;
            uint32_t total_blocks = fs->sb.blocks_count - fs->sb.first_data_block;
            uint32_t remaining = total_blocks - group * fs->sb.block_per_group;
            if (remaining < blocks_in_group) blocks_in_group = remaining;

            int free_bit = -1;
            for (uint32_t i = 0; i < blocks_in_group; ++i) {
                uint32_t byte_idx = i / 8;
                uint8_t bit_idx = i % 8;
                if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                    bitmap[byte_idx] |= (1 << bit_idx);
                    free_bit = static_cast<int>(i);
                    break;
                }
            }

            if (free_bit < 0) {
                Heap::kfree(bitmap);
                return 0;
            }

            if (!writeBlock(fs, fs->groups[group].block_bitmap, bitmap)) {
                Heap::kfree(bitmap);
                return 0;
            }
            Heap::kfree(bitmap);

            fs->groups[group].free_blocks_count--;
            fs->sb.free_blocks_count--;
            writeGroupDescriptors(fs);
            writeSuperblock(fs);

            uint32_t block_num = fs->sb.first_data_block + group * fs->sb.block_per_group + free_bit;
            return block_num;
        }

        uint32_t allocBlock(FsInstance *fs, uint32_t group_hint = 0) {
            for (uint32_t i = 0; i < fs->group_count; ++i) {
                uint32_t group = (group_hint + i) % fs->group_count;
                uint32_t block = allocBlockInGroup(fs, group);
                if (block != 0) return block;
            }
            return 0;
        }

        uint32_t allocInodeInGroup(FsInstance *fs, uint32_t group) {
            if (group >= fs->group_count) return 0;
            if (fs->groups[group].free_inodes_count == 0) return 0;

            uint8_t *bitmap = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!bitmap) return 0;

            if (!readBlock(fs, fs->groups[group].inode_bitmap, bitmap)) {
                Heap::kfree(bitmap);
                return 0;
            }

            int free_bit = -1;
            for (uint32_t i = 0; i < fs->sb.inodes_per_group; ++i) {
                uint32_t byte_idx = i / 8;
                uint8_t bit_idx = i % 8;
                if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                    bitmap[byte_idx] |= (1 << bit_idx);
                    free_bit = static_cast<int>(i);
                    break;
                }
            }

            if (free_bit < 0) {
                Heap::kfree(bitmap);
                return 0;
            }

            if (!writeBlock(fs, fs->groups[group].inode_bitmap, bitmap)) {
                Heap::kfree(bitmap);
                return 0;
            }
            Heap::kfree(bitmap);

            fs->groups[group].free_inodes_count--;
            fs->sb.free_inodes_count--;
            writeGroupDescriptors(fs);
            writeSuperblock(fs);

            uint32_t inode_num = group * fs->sb.inodes_per_group + free_bit + 1;
            return inode_num;
        }

        uint32_t allocInode(FsInstance *fs, uint32_t group_hint = 0) {
            for (uint32_t i = 0; i < fs->group_count; ++i) {
                uint32_t group = (group_hint + i) % fs->group_count;
                uint32_t inode = allocInodeInGroup(fs, group);
                if (inode != 0) return inode;
            }
            return 0;
        }

        bool writeBlock(FsInstance *fs, uint32_t block_num, const void *buf) {
            uint32_t sectors_per_block = fs->block_size / fs->dev->sector_size;
            uint64_t lba = static_cast<uint64_t>(block_num) * sectors_per_block;
            return fs->dev->write(fs->dev->device_handle, lba, sectors_per_block, buf);
        }

        bool writeSuperblock(FsInstance *fs) {
            uint8_t sb_buf[1024];
            memcpy(sb_buf, &fs->sb, sizeof(Superblock));
            return fs->dev->write(fs->dev->device_handle, 2, 1024 / fs->dev->sector_size, sb_buf);
        }

        bool writeGroupDescriptors(FsInstance *fs) {
            uint32_t bgdt_block = fs->sb.first_data_block + 1;
            uint32_t bgdt_bytes = sizeof(GroupDesc) * fs->group_count;
            uint32_t bgdt_blocks_needed = (bgdt_bytes + fs->block_size - 1) / fs->block_size;

            uint8_t *buf = static_cast<uint8_t *>(Heap::kmalloc(bgdt_blocks_needed * fs->block_size));
            if (!buf) return false;

            for (uint32_t i = 0; i < bgdt_blocks_needed * fs->block_size; ++i) buf[i] = 0;
            memcpy(buf, fs->groups, bgdt_bytes);

            bool ok = true;
            for (uint32_t i = 0; i < bgdt_blocks_needed && ok; ++i) {
                ok = writeBlock(fs, bgdt_block + i, buf + i * fs->block_size);
            }

            Heap::kfree(buf);
            return ok;
        }

        uint32_t inodeBlockAtOrAlloc(FsInstance *fs, Inode *inode, uint32_t index) {
            if (index < 12) {
                if (inode->block[index] == 0) {
                    uint32_t new_block = allocBlock(fs);
                    if (new_block == 0) return 0;
                    inode->block[index] = new_block;
                    inode->blocks += fs->block_size / 512;
                }
                return inode->block[index];
            }

            index -= 12;
            uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);
            if (index >= ptrs_per_block) {
                return 0;
            }

            if (inode->block[12] == 0) {
                uint32_t new_indirect = allocBlock(fs);
                if (new_indirect == 0) return 0;

                uint8_t *zero_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
                if (!zero_buf) return 0;
                for (uint32_t i = 0; i < fs->block_size; ++i) zero_buf[i] = 0;
                writeBlock(fs, new_indirect, zero_buf);
                Heap::kfree(zero_buf);

                inode->block[12] = new_indirect;
                inode->blocks += fs->block_size / 512;
            }

            uint32_t *indirect = static_cast<uint32_t *>(Heap::kmalloc(fs->block_size));
            if (!indirect) return 0;

            if (!readBlock(fs, inode->block[12], indirect)) {
                Heap::kfree(indirect);
                return 0;
            }

            uint32_t result = indirect[index];
            if (result == 0) {
                result = allocBlock(fs);
                if (result == 0) {
                    Heap::kfree(indirect);
                    return 0;
                }
                indirect[index] = result;
                writeBlock(fs, inode->block[12], indirect);
                inode->blocks += fs->block_size / 512;
            }

            Heap::kfree(indirect);
            return result;
        }

        uint32_t inodeBlockAt(FsInstance *fs, Inode *inode, uint32_t index) {
            if (index < 12) {
                return inode->block[index];
            }
            index -= 12;
            uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);
            if (index >= ptrs_per_block || inode->block[12] == 0) return 0;

            uint32_t *indirect = static_cast<uint32_t *>(Heap::kmalloc(fs->block_size));
            if (!indirect) return 0;

            uint32_t result = 0;
            if (readBlock(fs, inode->block[12], indirect)) {
                result = indirect[index];
            }
            Heap::kfree(indirect);
            return result;
        }

        bool freeBlock(FsInstance *fs, uint32_t block_num) {
            if (block_num == 0) return false;

            uint32_t relative = block_num - fs->sb.first_data_block;
            uint32_t group = relative / fs->sb.block_per_group;
            uint32_t index_in_group = relative % fs->sb.block_per_group;
            if (group >= fs->group_count) return false;

            uint8_t *bitmap = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!bitmap) return false;

            if (!readBlock(fs, fs->groups[group].block_bitmap, bitmap)) {
                Heap::kfree(bitmap);
                return false;
            }

            uint32_t byte_idx = index_in_group / 8;
            uint8_t bit_idx = index_in_group % 8;
            bitmap[byte_idx] &= ~(1 << bit_idx);

            bool ok = writeBlock(fs, fs->groups[group].block_bitmap, bitmap);
            Heap::kfree(bitmap);
            if (!ok) return false;

            fs->groups[group].free_blocks_count++;
            fs->sb.free_blocks_count++;
            writeGroupDescriptors(fs);
            writeSuperblock(fs);
            return true;
        }

        void freeAllInodeBlocks(FsInstance *fs, Inode *inode) {
            for (int i = 0; i < 12; ++i) {
                if (inode->block[i] != 0) {
                    freeBlock(fs, inode->block[i]);
                    inode->block[i] = 0;
                }
            }

            if (inode->block[12] != 0) {
                uint32_t *indirect = static_cast<uint32_t *>(Heap::kmalloc(fs->block_size));
                if (indirect) {
                    if (readBlock(fs, inode->block[12], indirect)) {
                        uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);
                        for (uint32_t i = 0; i < ptrs_per_block; ++i) {
                            if (indirect[i] != 0) freeBlock(fs, indirect[i]);
                        }
                    }
                    Heap::kfree(indirect);
                }
                freeBlock(fs, inode->block[12]);
                inode->block[12] = 0;
            }

            inode->size = 0;
            inode->blocks = 0;
        }

        bool removeDirEntry(FsInstance *fs, Inode *dir_inode, const char *name) {
            size_t name_len = strlen(name);

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return false;

            uint32_t num_blocks = (dir_inode->size + fs->block_size - 1) / fs->block_size;

            for (uint32_t b = 0; b < num_blocks; ++b) {
                uint32_t phys_block = inodeBlockAt(fs, dir_inode, b);
                if (phys_block == 0) continue;
                if (!readBlock(fs, phys_block, block_buf)) continue;

                uint32_t offset = 0;
                uint32_t prev_offset = 0;
                bool has_prev = false;

                while (offset < fs->block_size) {
                    auto *entry = reinterpret_cast<DirEntryOnDisk *>(block_buf + offset);
                    if (entry->rec_len == 0) break;

                    if (entry->inode != 0 && entry->name_len == name_len) {
                        const char *entry_name = reinterpret_cast<const char *>(entry) + sizeof(DirEntryOnDisk);
                        if (strncmp(entry_name, name, name_len) == 0) {
                            if (has_prev) {
                                auto *prev = reinterpret_cast<DirEntryOnDisk *>(block_buf + prev_offset);
                                prev->rec_len += entry->rec_len;
                            } else {
                                entry->inode = 0;
                            }

                            bool ok = writeBlock(fs, phys_block, block_buf);
                            Heap::kfree(block_buf);
                            return ok;
                        }
                    }

                    prev_offset = offset;
                    has_prev = true;
                    offset += entry->rec_len;
                }
            }

            Heap::kfree(block_buf);
            return false;
        }

        bool unlinkFile(void *fs_instance, const char *path) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            const char *last_slash = nullptr;
            for (const char *p = path; *p != '\0'; ++p) if (*p == '/') last_slash = p;

            char parent_path[128];
            const char *name;
            if (last_slash) {
                size_t plen = last_slash - path;
                if (plen >= sizeof(parent_path)) return false;
                memcpy(parent_path, path, plen);
                parent_path[plen] = '\0';
                name = last_slash + 1;
            } else {
                parent_path[0] = '\0';
                name = path;
            }
            if (name[0] == '\0') return false;

            uint32_t parent_inode_num = resolvePathToInode(fs, parent_path);
            if (parent_inode_num == 0) return false;

            Inode parent_inode;
            if (!readInode(fs, parent_inode_num, &parent_inode)) return false;

            uint32_t target_inode_num = lookupInDir(fs, &parent_inode, name);
            if (target_inode_num == 0) return false;

            Inode target_inode;
            if (!readInode(fs, target_inode_num, &target_inode)) return false;
            if ((target_inode.mode & S_IFMT) == S_IFDIR) return false;

            if (!removeDirEntry(fs, &parent_inode, name)) return false;
            writeInode(fs, parent_inode_num, &parent_inode);

            freeAllInodeBlocks(fs, &target_inode);

            target_inode.links_count = 0;
            target_inode.dtime = 0;
            writeInode(fs, target_inode_num, &target_inode);

            return freeInode(fs, target_inode_num);
        }

        bool rmdirPath(void *fs_instance, const char *path) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            const char *last_slash = nullptr;
            for (const char *p = path; *p != '\0'; ++p) if (*p == '/') last_slash = p;

            char parent_path[128];
            const char *name;
            if (last_slash) {
                size_t plen = last_slash - path;
                if (plen >= sizeof(parent_path)) return false;
                memcpy(parent_path, path, plen);
                parent_path[plen] = '\0';
                name = last_slash + 1;
            } else {
                parent_path[0] = '\0';
                name = path;
            }
            if (name[0] == '\0') return false;

            uint32_t parent_inode_num = resolvePathToInode(fs, parent_path);
            if (parent_inode_num == 0) return false;

            Inode parent_inode;
            if (!readInode(fs, parent_inode_num, &parent_inode)) return false;

            uint32_t target_inode_num = lookupInDir(fs, &parent_inode, name);
            if (target_inode_num == 0) return false;

            Inode target_inode;
            if (!readInode(fs, target_inode_num, &target_inode)) return false;
            if ((target_inode.mode & S_IFMT) != S_IFDIR) return false;

            uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
            if (!block_buf) return false;

            uint32_t num_blocks = (target_inode.size + fs->block_size - 1) / fs->block_size;
            bool has_children = false;

            for (uint32_t b = 0; b < num_blocks && !has_children; ++b) {
                uint32_t phys_block = inodeBlockAt(fs, &target_inode, b);
                if (phys_block == 0) continue;
                if (!readBlock(fs, phys_block, block_buf)) continue;

                uint32_t offset = 0;
                while (offset < fs->block_size) {
                    auto *entry = reinterpret_cast<DirEntryOnDisk *>(block_buf + offset);
                    if (entry->rec_len == 0) break;

                    if (entry->inode != 0) {
                        const char *entry_name = reinterpret_cast<const char *>(entry) + sizeof(DirEntryOnDisk);
                        bool is_dot = entry->name_len == 1 && entry_name[0] == '.';
                        bool is_dotdot = entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.';
                        if (!is_dot && !is_dotdot) {
                            has_children = true;
                            break;
                        }
                    }
                    offset += entry->rec_len;
                }
            }
            Heap::kfree(block_buf);

            if (has_children) return false;

            if (!removeDirEntry(fs, &parent_inode, name)) return false;
            parent_inode.links_count--;
            writeInode(fs, parent_inode_num, &parent_inode);

            freeAllInodeBlocks(fs, &target_inode);
            target_inode.links_count = 0;
            writeInode(fs, target_inode_num, &target_inode);

            return freeInode(fs, target_inode_num);
        }

        bool create(void *fs_instance, const char *path, bool is_directory) {
            auto *fs = static_cast<FsInstance *>(fs_instance);

            const char *last_slash = nullptr;
            for (const char *p = path; *p != '\0'; ++p) {
                if (*p == '/') last_slash = p;
            }

            char parent_path[128];
            const char *name;

            if (last_slash) {
                size_t plen = last_slash - path;
                if (plen >= sizeof(parent_path)) return false;
                memcpy(parent_path, path, plen);
                parent_path[plen] = '\0';
                name = last_slash + 1;
            } else {
                parent_path[0] = '\0';
                name = path;
            }

            if (name[0] == '\0') return false;

            uint32_t parent_inode_num = resolvePathToInode(fs, parent_path);
            if (parent_inode_num == 0) return false;

            Inode parent_inode;
            if (!readInode(fs, parent_inode_num, &parent_inode)) return false;
            if ((parent_inode.mode & S_IFMT) != S_IFDIR) return false;

            if (lookupInDir(fs, &parent_inode, name) != 0) {
                return false;
            }

            uint32_t new_inode_num = allocInode(fs);
            if (new_inode_num == 0) return false;

            Inode new_inode;
            for (size_t i = 0; i < sizeof(Inode); ++i) reinterpret_cast<uint8_t *>(&new_inode)[i] = 0;

            new_inode.mode = is_directory ? (S_IFDIR | 0755) : (S_IFREG | 0644);
            new_inode.links_count = is_directory ? 2 : 1;
            new_inode.size = 0;

            if (is_directory) {
                uint32_t data_block = allocBlock(fs);
                if (data_block == 0) return false;

                uint8_t *block_buf = static_cast<uint8_t *>(Heap::kmalloc(fs->block_size));
                if (!block_buf) return false;
                for (uint32_t i = 0; i < fs->block_size; ++i) block_buf[i] = 0;

                auto *dot = reinterpret_cast<DirEntryOnDisk *>(block_buf);
                dot->inode = new_inode_num;
                dot->rec_len = dirEntryMinSize(1);
                dot->name_len = 1;
                dot->file_type = FT_DIR;
                block_buf[sizeof(DirEntryOnDisk)] = '.';

                auto *dotdot = reinterpret_cast<DirEntryOnDisk *>(block_buf + dot->rec_len);
                dotdot->inode = parent_inode_num;
                dotdot->rec_len = static_cast<uint16_t>(fs->block_size - dot->rec_len);
                dotdot->name_len = 2;
                dotdot->file_type = FT_DIR;
                block_buf[dot->rec_len + sizeof(DirEntryOnDisk)] = '.';
                block_buf[dot->rec_len + sizeof(DirEntryOnDisk) + 1] = '.';

                bool ok = writeBlock(fs, data_block, block_buf);
                Heap::kfree(block_buf);
                if (!ok) return false;

                new_inode.block[0] = data_block;
                new_inode.size = fs->block_size;
                new_inode.blocks = fs->block_size / 512;

                parent_inode.links_count++;
            }

            if (!writeInode(fs, new_inode_num, &new_inode)) return false;

            uint8_t file_type = is_directory ? FT_DIR : FT_REG_FILE;
            if (!insertDirEntry(fs, &parent_inode, name, new_inode_num, file_type)) return false;

            return writeInode(fs, parent_inode_num, &parent_inode);
        }

        VFS::FilesystemDriver g_driver = {
            "ext2",
            mount,
            unmount,
            open,
            close,
            read,
            write,
            isDirectory,
            readdir,
            create,
            unlinkFile,
            rmdirPath
        };
    }

    VFS::FilesystemDriver *driver() {
        return &g_driver;
    }
}
