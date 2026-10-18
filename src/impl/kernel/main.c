#include "print.h"
#include "kprintf.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "fs.h"
#include "string.h"
#include "shell.h"
#include "io.h"

void kernel_main(uint64_t mbi_addr) {
    print_clear();
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    kprintf("SecaiDash OS\n");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    kprintf("Kernel V1.1\n\n");

    idt_init();
    pic_init();
    pit_init(PIT_HZ);
    keyboard_init();
    kprintf("[ok] IDT, PIC, PIT (%d Hz), keyboard\n", PIT_HZ);

    pmm_init(mbi_addr);
    heap_init();
    kprintf("[ok] memory: %lu MiB usable, %lu KiB heap in use\n",
            pmm_total_frames() * PAGE_SIZE / (1024 * 1024), heap_used() / 1024);

    fs_init();
    fs_mkdir("/home");
    fs_mkdir("/etc");
    fs_mkdir("/tmp");
    struct fs_node* f;
    if (fs_create("/etc/motd", &f) == FS_OK) {
        const char* motd = "Welcome to newOS!\n";
        fs_write(f, motd, strlen(motd), 0);
    }
    kprintf("[ok] ramfs mounted at /\n");

    cpu_sti();
    kprintf("Type 'help' for commands.\n\n");
    shell_run();
}
