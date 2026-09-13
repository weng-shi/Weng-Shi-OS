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

#include <syscall.h>
#include <terminal.h>
#include <keyboard.h>
#include <scheduler.h>
#include <vfs.h>
#include <heap.h>
#include <string.h>
#include <vmm.h>

#include "elf.h"
#include "printf.h"
#include "shell.h"
#include "usermode.h"

namespace {
    constexpr int STDIN_FD  = 0;
    constexpr int STDOUT_FD = 1;
    constexpr int STDERR_FD = 2;
    constexpr int FD_OFFSET = 3;

    int64_t sysWrite(int user_fd, uint64_t user_ptr, uint64_t length) {
        if (length > 4096) length = 4096;

        const char* str = reinterpret_cast<const char*>(user_ptr);

        if (user_fd == STDOUT_FD || user_fd == STDERR_FD) {
            for (uint64_t i = 0; i < length; ++i) {
                if (str[i] == '\0') break;
                g_terminal.putChar(str[i]);
            }
            return static_cast<int64_t>(length);
        }

        if (user_fd == STDIN_FD) return -1;

        int vfs_fd = user_fd - FD_OFFSET;
        return VFS::write(vfs_fd, str, static_cast<size_t>(length));
    }


    Scheduler::Task* g_pending_stdin_reader = nullptr;

    void wakeStdinReader() {
        if (g_pending_stdin_reader) {
            Scheduler::wake(g_pending_stdin_reader);
            g_pending_stdin_reader = nullptr;
        }
    }

    int64_t sysRead(int user_fd, uint64_t user_ptr, uint64_t length) {
        if (length > 4096) length = 4096;
        char* buf = reinterpret_cast<char*>(user_ptr);

        if (user_fd == STDIN_FD) {
            while (!Keyboard::hasChar()) {
                g_pending_stdin_reader = Scheduler::currentTask();
                Keyboard::setOnCharCallback(wakeStdinReader);
                Scheduler::blockCurrent();
            }

            uint64_t i = 0;
            while (i < length && Keyboard::hasChar()) {
                buf[i++] = Keyboard::getChar();
            }
            return static_cast<int64_t>(i);
        }

        if (user_fd == STDOUT_FD || user_fd == STDERR_FD) return -1;

        int vfs_fd = user_fd - FD_OFFSET;
        return VFS::read(vfs_fd, buf, static_cast<size_t>(length));
    }

    int64_t sysOpen(uint64_t user_path_ptr) {
        const char* path = reinterpret_cast<const char*>(user_path_ptr);
        int vfs_fd = VFS::open(path);
        if (vfs_fd == VFS::INVALID_FD) return -1;
        return vfs_fd + FD_OFFSET;
    }

    int64_t sysClose(int user_fd) {
        if (user_fd < FD_OFFSET) return -1;
        VFS::close(user_fd - FD_OFFSET);
        return 0;
    }

    uint64_t BRK_REGION_START = 0x50000000000ull;

    int64_t sysBrk(uint64_t requested_end) {
        Scheduler::Task* task = Scheduler::currentTask();
        if (!task) return -1;

        if (task->brk_start == 0) {
            task->brk_start = BRK_REGION_START + static_cast<uint64_t>(task->id) * 0x10000000ull;
            task->brk_current = task->brk_start;
        }

        if (requested_end == 0) {
            return static_cast<int64_t>(task->brk_current);
        }

        if (requested_end <= task->brk_current) {
            return static_cast<int64_t>(task->brk_current);
        }

        uint64_t old_page_end = (task->brk_current + VMM::PAGE_SIZE - 1) & ~(VMM::PAGE_SIZE - 1);
        uint64_t new_page_end = (requested_end + VMM::PAGE_SIZE - 1) & ~(VMM::PAGE_SIZE - 1);

        for (uint64_t addr = old_page_end; addr < new_page_end; addr += VMM::PAGE_SIZE) {
            if (!VMM::allocAndMapPage(addr, VMM::PAGE_WRITABLE | VMM::PAGE_USER)) {
                return static_cast<int64_t>(task->brk_current);
            }
        }

        task->brk_current = requested_end;
        return static_cast<int64_t>(task->brk_current);
    }

    [[noreturn]] void sysExit(uint64_t exit_code) {
        Shell::g_input_row = g_terminal.getRow();
        Shell::g_input_col = g_terminal.getColumn();
        Scheduler::exitTask(exit_code);
    }

    struct SyscallDirEntry {
        char name[64];
        uint8_t is_directory;
    };

    int64_t sysReaddir(uint64_t path_ptr, int index, uint64_t out_entry_ptr) {
        const char* path = reinterpret_cast<const char*>(path_ptr);
        auto* out = reinterpret_cast<SyscallDirEntry*>(out_entry_ptr);

        VFS::DirEntry entry;
        if (!VFS::listDirectory(path, index, &entry)) {
            return 0;
        }

        memcpy(out->name, entry.name, sizeof(out->name));
        out->is_directory = entry.is_directory ? 1 : 0;
        return 1;
    }

    constexpr int MAX_EXEC_ARGS = 16;

    bool copyUserArgv(uint64_t argv_user_ptr, int *out_argc, char storage[][64], char **out_argv) {
        auto *user_argv = reinterpret_cast<const uint64_t *>(argv_user_ptr);
        int argc = 0;

        while (argc < MAX_EXEC_ARGS) {
            uint64_t str_ptr = user_argv[argc];
            if (str_ptr == 0) break;

            const char *str = reinterpret_cast<const char *>(str_ptr);
            size_t len = strlen(str);
            if (len >= 64) len = 63;
            memcpy(storage[argc], str, len);
            storage[argc][len] = '\0';
            out_argv[argc] = storage[argc];

            ++argc;
        }

        *out_argc = argc;
        return argc > 0;
    }

    void sysExec(Registers *regs, uint64_t path_ptr, uint64_t argv_ptr) {
        const char *path = reinterpret_cast<const char *>(path_ptr);

        static char argv_storage[MAX_EXEC_ARGS][64];
        static char *argv[MAX_EXEC_ARGS];
        int argc = 0;

        if (!copyUserArgv(argv_ptr, &argc, argv_storage, argv)) {
            regs->rax = static_cast<uint64_t>(-1);
            return;
        }

        uint64_t stack_top = 0;
        uint64_t argv_user_ptr = 0;
        uint64_t entry = Elf::load(path, argc, argv, &stack_top, &argv_user_ptr);

        if (entry == 0) {
            regs->rax = static_cast<uint64_t>(-1);
            return;
        }

        regs->rip = entry;
        regs->rsp = stack_top;
        regs->rdi = static_cast<uint64_t>(argc);
        regs->rsi = argv_user_ptr;
    }

    int64_t sysWait(uint32_t task_id) {
        return static_cast<int64_t>(Scheduler::waitForTask(task_id));
    }

    constexpr int MAX_SPAWN_ARGS = 16;

    struct SpawnParams {
        char path[128];
        char argv_storage[MAX_SPAWN_ARGS][64];
        char *argv[MAX_SPAWN_ARGS];
        int argc;
    };

    SpawnParams g_spawn_params;

    void spawnedTaskEntry() {
        char path[128];
        char argv_storage[MAX_SPAWN_ARGS][64];
        char *argv[MAX_SPAWN_ARGS];
        int argc = g_spawn_params.argc;

        memcpy(path, g_spawn_params.path, sizeof(path));
        for (int i = 0; i < argc; ++i) {
            memcpy(argv_storage[i], g_spawn_params.argv_storage[i], sizeof(argv_storage[i]));
            argv[i] = argv_storage[i];
        }

        uint64_t stack_top = 0;
        uint64_t argv_user_ptr = 0;
        uint64_t entry = Elf::load(path, argc, argv, &stack_top, &argv_user_ptr);

        if (!entry) {
            printf("%r[spawn] failed to load ELF %s\n%r", VGA::RED, path, VGA::WHITE);
            Scheduler::exitTask(-1);
        }

        Usermode::enter(entry, stack_top, argc, argv_user_ptr);
    }

    int64_t sysSpawn(uint64_t path_ptr, uint64_t argv_ptr) {
        const char *path = reinterpret_cast<const char *>(path_ptr);

        size_t path_len = strlen(path);
        if (path_len >= sizeof(g_spawn_params.path)) return -1;
        memcpy(g_spawn_params.path, path, path_len + 1);

        auto *user_argv = reinterpret_cast<const uint64_t *>(argv_ptr);
        int argc = 0;
        while (argc < MAX_SPAWN_ARGS) {
            uint64_t str_ptr = user_argv[argc];
            if (!str_ptr) break;

            const char *str = reinterpret_cast<const char *>(str_ptr);
            size_t len = strlen(str);
            if (len >= 64) len = 63;
            memcpy(g_spawn_params.argv_storage[argc], str, len);
            g_spawn_params.argv_storage[argc][len] = '\0';
            g_spawn_params.argv[argc] = g_spawn_params.argv_storage[argc];
            ++argc;
        }
        g_spawn_params.argc = argc;

        Scheduler::Task *task = Scheduler::createTask(spawnedTaskEntry);
        if (!task) return -1;

        return static_cast<int64_t>(task->id);
    }
}

extern "C" void syscall_dispatch(Registers* regs) {
    switch (regs->rax) {
        case SYS_WRITE:
            regs->rax = static_cast<uint64_t>(sysWrite(static_cast<int>(regs->rdi), regs->rsi, regs->rdx));
            break;

        case SYS_READ:
            regs->rax = static_cast<uint64_t>(sysRead(static_cast<int>(regs->rdi), regs->rsi, regs->rdx));
            break;

        case SYS_OPEN:
            regs->rax = static_cast<uint64_t>(sysOpen(regs->rdi));
            break;

        case SYS_CLOSE:
            regs->rax = static_cast<uint64_t>(sysClose(static_cast<int>(regs->rdi)));
            break;

        case SYS_BRK:
            regs->rax = static_cast<uint64_t>(sysBrk(regs->rdi));
            break;

        case SYS_EXIT:
            sysExit(regs->rdi);
            break;

        case SYS_READDIR:
            regs->rax = static_cast<uint64_t>(
                sysReaddir(regs->rdi, static_cast<int>(regs->rsi), regs->rdx));
            break;

        case SYS_EXEC:
            sysExec(regs, regs->rdi, regs->rsi);
            break;

        case SYS_SPAWN:
            regs->rax = static_cast<uint64_t>(sysSpawn(regs->rdi, regs->rsi));
            break;

        case SYS_WAIT:
            regs->rax = static_cast<uint64_t>(sysWait(static_cast<uint32_t>(regs->rdi)));
            break;

        default:
            g_terminal.write("[syscall] unknown syscall number: ");
            g_terminal.writeDec(regs->rax);
            g_terminal.write("\n");
            regs->rax = static_cast<uint64_t>(-1);
            break;
    }
}