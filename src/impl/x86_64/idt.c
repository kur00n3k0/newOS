#include "idt.h"
#include "pic.h"
#include "print.h"
#include "kprintf.h"
#include "io.h"

#define IDT_ENTRIES 256
#define KERNEL_CS 0x08
#define GATE_INTERRUPT 0x8E // present, ring 0, 64-bit interrupt gate

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void* isr_stub_table[];

static struct idt_entry idt[IDT_ENTRIES];
static irq_handler_t irq_handlers[16];

static const char* exception_names[32] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range exceeded",
    "Invalid opcode", "Device not available", "Double fault", "Coprocessor overrun",
    "Invalid TSS", "Segment not present", "Stack-segment fault", "General protection fault",
    "Page fault", "Reserved", "x87 FPU error", "Alignment check", "Machine check",
    "SIMD FP exception", "Virtualization exception", "Control protection", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor injection",
    "VMM communication", "Security exception", "Reserved",
};

static void idt_set_gate(uint8_t vector, void* handler) {
    uint64_t addr = (uint64_t) handler;
    idt[vector] = (struct idt_entry) {
        .offset_low = addr & 0xFFFF,
        .selector = KERNEL_CS,
        .ist = 0,
        .type_attr = GATE_INTERRUPT,
        .offset_mid = (addr >> 16) & 0xFFFF,
        .offset_high = (uint32_t) (addr >> 32),
        .zero = 0,
    };
}

void idt_init(void) {
    for (int i = 0; i < 48; i++) {
        idt_set_gate((uint8_t) i, isr_stub_table[i]);
    }

    struct idt_pointer ptr = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t) idt,
    };
    __asm__ volatile ("lidt %0" : : "m"(ptr));
}

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

static void exception_handler(struct regs* r) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_RED);
    kprintf("\n*** EXCEPTION %d: %s ***\n", (int) r->int_no, exception_names[r->int_no]);
    kprintf("err=%lx  rip=%lx  cs=%lx  rflags=%lx\n", r->err_code, r->rip, r->cs, r->rflags);
    kprintf("rax=%016lx rbx=%016lx rcx=%016lx\n", r->rax, r->rbx, r->rcx);
    kprintf("rdx=%016lx rsi=%016lx rdi=%016lx\n", r->rdx, r->rsi, r->rdi);
    kprintf("rbp=%016lx rsp=%016lx cr2=%016lx\n", r->rbp, r->rsp, cr2);
    kprintf("System halted.\n");

    for (;;) {
        cpu_cli();
        cpu_hlt();
    }
}

void isr_handler(struct regs* r) {
    if (r->int_no < 32) {
        exception_handler(r);
        return;
    }

    uint8_t irq = (uint8_t) (r->int_no - IRQ_BASE);

    if (pic_is_spurious(irq)) {
        return;
    }

    if (irq_handlers[irq]) {
        irq_handlers[irq](r);
    }

    pic_eoi(irq);
}
