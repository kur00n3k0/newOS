#include "shell.h"
#include "keyboard.h"
#include "kprintf.h"
#include "print.h"
#include "pit.h"
#include "pmm.h"
#include "heap.h"
#include "string.h"
#include "io.h"
#include <stdint.h>

#define LINE_MAX 128
#define ARGS_MAX 8

struct command {
    const char* name;
    const char* help;
    void (*run)(int argc, char** argv);
};

static const struct command commands[];

static unsigned long parse_uint(const char* s) {
    unsigned long n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (unsigned long) (*s++ - '0');
    }
    return n;
}

static void cmd_help(int argc, char** argv) {
    (void) argc; (void) argv;
    for (const struct command* c = commands; c->name; c++) {
        kprintf("  %-8s %s\n", c->name, c->help);
    }
}

static void cmd_clear(int argc, char** argv) {
    (void) argc; (void) argv;
    print_clear();
}

static void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s%s", i > 1 ? " " : "", argv[i]);
    }
    kprintf("\n");
}

static void cmd_uptime(int argc, char** argv) {
    (void) argc; (void) argv;
    uint64_t secs = pit_ticks() / PIT_HZ;
    kprintf("up %lu:%02lu:%02lu (%lu ticks)\n", secs / 3600, secs / 60 % 60, secs % 60, pit_ticks());
}

static void cmd_mem(int argc, char** argv) {
    (void) argc; (void) argv;
    kprintf("physical: %lu KiB total, %lu KiB free\n",
            pmm_total_frames() * PAGE_SIZE / 1024, pmm_free_frames() * PAGE_SIZE / 1024);
    kprintf("heap:     %lu KiB used, %lu KiB free\n", heap_used() / 1024, heap_free() / 1024);
}

static void cmd_alloc(int argc, char** argv) {
    size_t n = argc > 1 ? parse_uint(argv[1]) : 64;
    uint8_t* p = kmalloc(n);
    if (!p) {
        kprintf("kmalloc(%lu) failed\n", n);
        return;
    }
    memset(p, 0xAB, n);
    kprintf("kmalloc(%lu) = %p, written and freed\n", n, p);
    kfree(p);
}

// Allocates and frees in an interleaved order, then checks the heap coalesced back.
static void cmd_heaptest(int argc, char** argv) {
    (void) argc; (void) argv;
    size_t before = heap_free();
    void* a = kmalloc(100);
    void* b = kmalloc(5000);
    void* c = kmalloc(33);
    void* d = kcalloc(10, 8);
    kprintf("a=%p b=%p c=%p d=%p\n", a, b, c, d);
    kfree(b);
    kfree(a);
    kfree(d);
    kfree(c);
    size_t after = heap_free();
    kprintf("heap free before=%lu after=%lu: %s\n", before, after, before == after ? "OK" : "FAIL");
}

static void cmd_color(int argc, char** argv) {
    if (argc < 2) {
        kprintf("usage: color <fg 0-15> [bg 0-15]\n");
        return;
    }
    print_set_color((uint8_t) (parse_uint(argv[1]) & 15), argc > 2 ? (uint8_t) (parse_uint(argv[2]) & 15) : PRINT_COLOR_BLACK);
}

static void cmd_crash(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "pf") == 0) {
        *(volatile int*) 0x40000000 = 1; // first address past the identity map
    } else if (argc > 1 && strcmp(argv[1], "div") == 0) {
        // inline asm: a C division by zero is UB and the compiler may fold it away
        __asm__ volatile ("xor %%edx, %%edx; mov $1, %%eax; xor %%ecx, %%ecx; div %%ecx" : : : "eax", "ecx", "edx");
    } else {
        __asm__ volatile ("ud2");
    }
}

static void cmd_reboot(int argc, char** argv) {
    (void) argc; (void) argv;
    while (inb(0x64) & 2) {
    }
    outb(0x64, 0xFE); // pulse the CPU reset line via the keyboard controller
    for (;;) {
        cpu_hlt();
    }
}

static void cmd_halt(int argc, char** argv) {
    (void) argc; (void) argv;
    kprintf("System halted.\n");
    for (;;) {
        cpu_cli();
        cpu_hlt();
    }
}

static const struct command commands[] = {
    { "help",     "list commands", cmd_help },
    { "clear",    "clear the screen", cmd_clear },
    { "echo",     "print arguments", cmd_echo },
    { "uptime",   "time since boot", cmd_uptime },
    { "mem",      "physical memory and heap usage", cmd_mem },
    { "alloc",    "alloc [bytes]: kmalloc/kfree smoke test", cmd_alloc },
    { "heaptest", "heap split/coalesce test", cmd_heaptest },
    { "color",    "color <fg> [bg]: set text colours", cmd_color },
    { "crash",    "crash [ud|div|pf]: trigger a CPU exception", cmd_crash },
    { "reboot",   "restart the machine", cmd_reboot },
    { "halt",     "stop the CPU", cmd_halt },
    { 0, 0, 0 },
};

static void read_line(char* line) {
    size_t len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            print_char('\n');
            line[len] = '\0';
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                print_char('\b');
            }
            continue;
        }
        if (c >= ' ' && len < LINE_MAX - 1) {
            line[len++] = c;
            print_char(c);
        }
    }
}

static void execute(char* line) {
    char* argv[ARGS_MAX];
    int argc = 0;

    // split in place on spaces
    for (char* p = line; *p && argc < ARGS_MAX; ) {
        while (*p == ' ') {
            *p++ = '\0';
        }
        if (!*p) {
            break;
        }
        argv[argc++] = p;
        while (*p && *p != ' ') {
            p++;
        }
    }
    if (argc == 0) {
        return;
    }

    for (const struct command* c = commands; c->name; c++) {
        if (strcmp(argv[0], c->name) == 0) {
            c->run(argc, argv);
            return;
        }
    }
    kprintf("unknown command: %s (try 'help')\n", argv[0]);
}

void shell_run(void) {
    char line[LINE_MAX];
    for (;;) {
        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        kprintf("newOS> ");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        read_line(line);
        execute(line);
    }
}
