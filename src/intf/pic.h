#pragma once

#include <stdint.h>

#define IRQ_BASE 32

void pic_init(void);
void pic_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq, int masked);
int pic_is_spurious(uint8_t irq);
