#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include <stdbool.h>

#define KBD_DATA 0x60
#define BUFFER_SIZE 256 // power of two

// US layout, scancode set 1, indexed by make code
static const char map_normal[] =
    "\0\033" "1234567890-=\b\t" "qwertyuiop[]\n\0" "asdfghjkl;'`\0\\" "zxcvbnm,./\0*\0 ";
static const char map_shift[] =
    "\0\033" "!@#$%^&*()_+\b\t" "QWERTYUIOP{}\n\0" "ASDFGHJKL:\"~\0|" "ZXCVBNM<>?\0*\0 ";

_Static_assert(sizeof(map_normal) == 0x3A + 1, "normal scancode map size");
_Static_assert(sizeof(map_shift) == 0x3A + 1, "shifted scancode map size");

static volatile char buffer[BUFFER_SIZE];
static volatile unsigned head = 0;
static volatile unsigned tail = 0;

static bool shift = false;
static bool caps = false;
static bool extended = false;

static void push(char c) {
    unsigned next = (head + 1) & (BUFFER_SIZE - 1);
    if (next == tail) {
        return; // full, drop
    }
    buffer[head] = c;
    head = next;
}

static void keyboard_irq(struct regs* r) {
    (void) r;
    uint8_t code = inb(KBD_DATA);

    if (code == 0xE0) {
        extended = true;
        return;
    }
    if (extended) { // arrows, delete, etc. are ignored for now
        extended = false;
        return;
    }

    bool release = code & 0x80;
    code &= 0x7F;

    if (code == 0x2A || code == 0x36) {
        shift = !release;
        return;
    }
    if (release) {
        return;
    }
    if (code == 0x3A) {
        caps = !caps;
        return;
    }
    if (code >= sizeof(map_normal)) {
        return;
    }

    char c = shift ? map_shift[code] : map_normal[code];
    if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
        c ^= 0x20;
    }
    if (c) {
        push(c);
    }
}

void keyboard_init(void) {
    while (inb(0x64) & 1) { // drain anything the firmware left behind
        inb(KBD_DATA);
    }
    irq_register(1, keyboard_irq);
    pic_set_mask(1, 0);
}

char keyboard_getchar(void) {
    for (;;) {
        cpu_cli();
        if (head != tail) {
            char c = buffer[tail];
            tail = (tail + 1) & (BUFFER_SIZE - 1);
            cpu_sti();
            return c;
        }
        // sti only takes effect after the next instruction, so no wakeup is lost
        __asm__ volatile ("sti; hlt");
    }
}
