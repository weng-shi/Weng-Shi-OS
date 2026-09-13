#include <panic.h>
#include <terminal.h>
#include <serial.h>
#include <scheduler.h>
#include <printf.h>

namespace {
  const char* exception_messages[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Hypervisor Injection", "VMM Communication", "Security Exception"
  };

  uint64_t readCR2() {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
  }
}

[[noreturn]] void kernel_panic(Registers* regs) {
  const char* name = (regs->vector < 32) ? exception_messages[regs->vector] : "Unknown";

  printf("%r\n*** KERNEL PANIC ***\n\n", VGA::RED);

  printf("Exception: %s\n", name);

  if (regs->vector == 14) {
    printf("Faulting address (CR2): %x\n", readCR2());
    printf("Reason: %s, %s, %s\n",
           (regs->err_code & 1) ? "protection violation" : "page not present",
           (regs->err_code & 2) ? "write" : "read",
           (regs->err_code & 4) ? "user mode" : "kernel mode");
  }

  printf("Vector:   %x\n", regs->vector);
  printf("Error:    %x\n", regs->err_code);
  printf("RIP:      %x\n", regs->rip);
  printf("CS:       %x\n", regs->cs);
  printf("RFLAGS:   %x\n", regs->rflags);
  printf("RSP:      %x\n", regs->rsp);
  printf("RAX:      %x\n", regs->rax);
  printf("RBX:      %x\n", regs->rbx);
  printf("RCX:      %x\n", regs->rcx);
  printf("RDX:      %x\n", regs->rdx);
  printf("%r", VGA::WHITE);

  Serial::write("KERNEL PANIC: ");
  Serial::write(name);
  Serial::write("\n");

  asm volatile("cli");
  for (;;) asm volatile("hlt");
}

[[noreturn]] void kernel_panic_user(Registers* regs) {
  const char* name = (regs->vector < 32) ? exception_messages[regs->vector] : "Unknown";

  printf("%r\n*** Program crashed: %s ***\n", VGA::YELLOW, name);

  if (regs->vector == 14) {
    printf("Faulting address: %x\n", readCR2());
    printf("%s, %s\n",
           (regs->err_code & 1) ? "Protection violation" : "Page not present",
           (regs->err_code & 2) ? "write" : "read");
  }

  printf("RIP: %x\n", regs->rip);
  printf("%r", VGA::WHITE);

  Serial::write("[user exception] ");
  Serial::write(name);
  Serial::write(" — killing task\n");

  Scheduler::exitTask(-1);
}
