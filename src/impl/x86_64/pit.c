#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182

static volatile uint64_t ticks = 0;

static void pit_irq(struct regs* r) {
    (void) r;
    ticks++;
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_HZ / hz;

    outb(PIT_COMMAND, 0x36); // channel 0, lobyte/hibyte, rate generator
    outb(PIT_CHANNEL0, (uint8_t) (divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t) (divisor >> 8));

    irq_register(0, pit_irq);
    pic_set_mask(0, 0);
}

uint64_t pit_ticks(void) {
    return ticks;
}

void pit_sleep_ms(uint32_t ms) {
    uint64_t target = ticks + (uint64_t) ms * PIT_HZ / 1000 + 1;
    while (ticks < target) {
        cpu_hlt();
    }
}
