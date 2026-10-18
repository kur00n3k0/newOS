#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20
#define PIC_READ_ISR 0x0B

void pic_init(void) {
    // ICW1: start initialisation, expect ICW4
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    // ICW2: vector offsets (IRQ 0-7 -> 32-39, IRQ 8-15 -> 40-47)
    outb(PIC1_DATA, IRQ_BASE); io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8); io_wait();
    // ICW3: slave on IRQ2
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // mask everything except the cascade line; drivers unmask their own IRQ
    outb(PIC1_DATA, 0xFB);
    outb(PIC2_DATA, 0xFF);
}

void pic_set_mask(uint8_t irq, int masked) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq < 8 ? irq : irq - 8;
    uint8_t mask = inb(port);

    if (masked) {
        mask |= (uint8_t) (1 << bit);
    } else {
        mask &= (uint8_t) ~(1 << bit);
    }
    outb(port, mask);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

// IRQ7/IRQ15 can fire spuriously; they are real only if set in the in-service register.
int pic_is_spurious(uint8_t irq) {
    if (irq == 7) {
        outb(PIC1_CMD, PIC_READ_ISR);
        return !(inb(PIC1_CMD) & 0x80);
    }
    if (irq == 15) {
        outb(PIC2_CMD, PIC_READ_ISR);
        if (!(inb(PIC2_CMD) & 0x80)) {
            outb(PIC1_CMD, PIC_EOI); // master still saw the cascade IRQ
            return 1;
        }
    }
    return 0;
}
