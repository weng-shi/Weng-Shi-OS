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

#include <scheduler.h>
#include <heap.h>
#include <gdt.h>
#include <terminal.h>
#include <vmm.h>

#include "keyboard.h"

extern "C" void scheduler_enter(uint64_t rsp);

extern "C" uint64_t g_next_task_rsp;

namespace Scheduler {
    namespace {
        Task *g_current = nullptr;
        Task *g_tail = nullptr;
        uint32_t g_next_id = 1;
        bool g_started = false;
        constexpr int MAX_PENDING_CLEANUP = 8;
        Task *g_pending_cleanup[MAX_PENDING_CLEANUP];
        int g_pending_cleanup_count = 0;

        void reapTerminated() {
            for (int i = 0; i < g_pending_cleanup_count; ++i) {
                Task *t = g_pending_cleanup[i];
                Heap::kfree(t->stack_base);
                t->stack_base = nullptr;
            }
            g_pending_cleanup_count = 0;
        }

        Task *findNextReady(Task *from) {
            Task *candidate = from->next;
            Task *start = candidate;

            do {
                if (candidate->state == TaskState::Ready) return candidate;
                candidate = candidate->next;
            } while (candidate != start);

            return nullptr; // nothing ready anywhere in the ring
        }

        [[noreturn]] void triggerReschedule() {
            asm volatile("int $32");
            __builtin_unreachable();
        }

        Task* findTaskById(uint32_t id) {
            if (!g_tail) return nullptr;

            Task* start = g_tail->next;
            Task* candidate = start;
            do {
                if (candidate->id == id) return candidate;
                candidate = candidate->next;
            } while (candidate != start);

            return nullptr;
        }

    }

    void init() {
        g_current = nullptr;
        g_tail = nullptr;
        g_next_id = 1;
        g_started = false;
        g_pending_cleanup_count = 0;
    }

    Task *createTask(TaskFunc entry, size_t stack_size) {
        auto *task = static_cast<Task *>(Heap::kmalloc(sizeof(Task)));
        auto *stack = static_cast<uint8_t *>(Heap::kmalloc(stack_size));
        auto* kstack = static_cast<uint8_t*>(Heap::kmalloc(stack_size));
        if (!task || !stack || !kstack) return nullptr;

		uint64_t new_plm4 = VMM::createAddressSpace();
		if (new_plm4 == 0) return nullptr;

        uint64_t stack_top = reinterpret_cast<uint64_t>(stack) + stack_size;
        uint64_t frame_addr = (stack_top - sizeof(Registers)) & ~0xFull;
        uint64_t kstack_top = reinterpret_cast<uint64_t>(kstack) + stack_size;
        kstack_top &= ~0xFull;

        auto *frame = reinterpret_cast<Registers *>(frame_addr);
        for (size_t i = 0; i < sizeof(Registers); ++i) {
            reinterpret_cast<uint8_t *>(frame)[i] = 0;
        }

        frame->rip = reinterpret_cast<uint64_t>(entry);
        frame->cs = GDT_KERNEL_CODE;
        frame->rflags = 0x202;
        frame->rsp = frame_addr;
        frame->ss = GDT_KERNEL_DATA;

        task->rsp = frame_addr;
        task->id = g_next_id++;
        task->state = TaskState::Ready;
        task->stack_base = stack;
        task->kernel_stack_top = kstack_top;

        task->brk_start = 0;
        task->brk_current = 0;

        task->exit_code = 0;
        task->waiting_parent = nullptr;

		task->plm4_phys = new_plm4;

        if (!g_tail) {
            task->next = task;
            g_tail = task;
        } else {
            task->next = g_tail->next;
            g_tail->next = task;
            g_tail = task;
        }

        return task;
    }

    [[noreturn]] void start() {
        g_current = g_tail->next;
        g_current->state = TaskState::Running;
        g_started = true;
        gdt_set_kernel_stack(g_current->kernel_stack_top);

		VMM::switchAddressSpace(g_current->plm4_phys);

        scheduler_enter(g_current->rsp);
        for (;;) {}
    }

    Task *currentTask() {
        return g_current;
    }

    void tick(Registers* regs) {
        if (!g_started || !g_current) return;
        reapTerminated();

        g_current->rsp = reinterpret_cast<uint64_t>(regs);
        if (g_current->state == TaskState::Running) g_current->state = TaskState::Ready;

        Task* next = findNextReady(g_current);
        if (!next) {
            g_next_task_rsp = g_current->rsp;
            g_current->state = TaskState::Running;
            return;
        }

        g_current = next;
        g_current->state = TaskState::Running;
        g_next_task_rsp = g_current->rsp;

        gdt_set_kernel_stack(g_current->kernel_stack_top);
		VMM::switchAddressSpace(g_current->plm4_phys);
    }

    void yield() {
        asm volatile("int $32");
    }

    [[noreturn]] void exitTask(int32_t exit_code) {
        killTask(g_current->id, exit_code);
        __builtin_unreachable();
    }

    void blockCurrent() {
        g_current->state = TaskState::Blocked;
        yield();
    }

    void wake(Task *task) {
        if (task->state == TaskState::Blocked) {
            task->state = TaskState::Ready;
        }
    }

    bool killTask(uint32_t id, int32_t exit_code = 0) {
        Task* target = findTaskById(id);
        if (!target || target->state == TaskState::Terminated) return false;

        bool killing_self = (target == g_current);

        target->exit_code = exit_code;
        target->state = TaskState::Terminated;

        if (target->waiting_parent) {
            wake(target->waiting_parent);
            target->waiting_parent = nullptr;
        } else {
            if (g_pending_cleanup_count < MAX_PENDING_CLEANUP) {
                g_pending_cleanup[g_pending_cleanup_count++] = target;
            }
        }

        if (g_pending_cleanup_count < MAX_PENDING_CLEANUP) {
            g_pending_cleanup[g_pending_cleanup_count++] = target;
        }

        if (Keyboard::getFocus() == id) {
            Keyboard::setFocus(0);
        }

        if (killing_self) {
            asm volatile("sti");
            for (;;) {
                asm volatile("hlt");
            }
        }

        return true;
    }

    int32_t waitForTask(uint32_t id) {
        Task *target = findTaskById(id);
        if (!target) return -1;

        if (target->state != TaskState::Terminated) {
            target->waiting_parent = g_current;
            blockCurrent();
        }

        int32_t exit_code = target->exit_code;
        return exit_code;
    }
}
