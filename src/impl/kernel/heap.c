#include "heap.h"
#include "pmm.h"
#include "kprintf.h"
#include "string.h"
#include <stdint.h>

#define ALIGNMENT 16
#define BLOCK_MAGIC 0xB10CB10C
#define MIN_HEAP_PAGES 16     // 64 KiB
#define WANTED_HEAP_PAGES 2048 // 8 MiB

struct block {
    size_t size;        // payload size in bytes
    struct block* prev; // physical neighbours, so free blocks can be coalesced
    struct block* next;
    uint32_t free;
    uint32_t magic;
};

_Static_assert(sizeof(struct block) % ALIGNMENT == 0, "block header must keep payloads aligned");

static struct block* first = NULL;
static uint8_t* heap_end = NULL;

static size_t round_up(size_t n) {
    return (n + ALIGNMENT - 1) & ~(size_t) (ALIGNMENT - 1);
}

void heap_init(void) {
    size_t pages = WANTED_HEAP_PAGES;
    uint64_t base = 0;

    // identity-mapped, so a physical run doubles as a virtual one
    while (pages >= MIN_HEAP_PAGES && !(base = pmm_alloc_contiguous(pages))) {
        pages /= 2;
    }
    if (!base) {
        kprintf("heap: out of memory, kmalloc disabled\n");
        return;
    }

    first = (struct block*) base;
    first->size = pages * PAGE_SIZE - sizeof(struct block);
    first->prev = NULL;
    first->next = NULL;
    first->free = 1;
    first->magic = BLOCK_MAGIC;
    heap_end = (uint8_t*) base + pages * PAGE_SIZE;
}

void* kmalloc(size_t size) {
    if (size == 0 || !first) {
        return NULL;
    }
    size = round_up(size);

    for (struct block* b = first; b; b = b->next) {
        if (!b->free || b->size < size) {
            continue;
        }

        // split if the remainder can hold a header plus a minimal payload
        if (b->size >= size + sizeof(struct block) + ALIGNMENT) {
            struct block* rest = (struct block*) ((uint8_t*) (b + 1) + size);
            rest->size = b->size - size - sizeof(struct block);
            rest->prev = b;
            rest->next = b->next;
            rest->free = 1;
            rest->magic = BLOCK_MAGIC;
            if (b->next) {
                b->next->prev = rest;
            }
            b->next = rest;
            b->size = size;
        }

        b->free = 0;
        return b + 1;
    }
    return NULL;
}

void* kcalloc(size_t count, size_t size) {
    if (size && count > (size_t) -1 / size) {
        return NULL;
    }
    void* p = kmalloc(count * size);
    if (p) {
        memset(p, 0, count * size);
    }
    return p;
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }

    struct block* b = (struct block*) ptr - 1;
    if ((uint8_t*) b < (uint8_t*) first || (uint8_t*) b >= heap_end || b->magic != BLOCK_MAGIC) {
        kprintf("kfree: invalid pointer %p\n", ptr);
        return;
    }
    if (b->free) {
        kprintf("kfree: double free of %p\n", ptr);
        return;
    }
    b->free = 1;

    if (b->next && b->next->free) {
        b->size += sizeof(struct block) + b->next->size;
        b->next = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(struct block) + b->size;
        b->prev->next = b->next;
        if (b->next) {
            b->next->prev = b->prev;
        }
    }
}

size_t heap_used(void) {
    size_t n = 0;
    for (struct block* b = first; b; b = b->next) {
        if (!b->free) {
            n += b->size;
        }
    }
    return n;
}

size_t heap_free(void) {
    size_t n = 0;
    for (struct block* b = first; b; b = b->next) {
        if (b->free) {
            n += b->size;
        }
    }
    return n;
}
