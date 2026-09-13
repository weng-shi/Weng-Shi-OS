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
#include <idt.h>

namespace Scheduler {

  using TaskFunc = void (*)();

  enum class TaskState { Ready, Running, Blocked, Terminated };

  struct Task {
	uint64_t rsp;
	uint32_t id;
	TaskState state;
	uint8_t* stack_base;
	uint64_t kernel_stack_top;
	uint64_t brk_start;
	uint64_t brk_current;
	int32_t exit_code;
	Task *waiting_parent;
	uint64_t plm4_phys;
	Task *next;
  };

  void init();
  Task* createTask(TaskFunc entry, size_t stack_size = 16384);
  [[noreturn]] void start();
  void tick(Registers* regs);

  void yield();

  [[noreturn]] void exitTask(int32_t exit_code = 0);

  void blockCurrent();

  void wake(Task* task);

  Task* currentTask();

  bool killTask(uint32_t id, int32_t exit_code);

  int32_t waitForTask(uint32_t id);

}
