#pragma once

#include <stdint.h>

// Register state pushed by isr_common (order matches the push sequence in isr.asm).
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct regs*);

void idt_init(void);
void irq_register(uint8_t irq, irq_handler_t handler);
