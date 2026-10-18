#include "pmm.h"
#include "multiboot2.h"
#include "kprintf.h"
#include "string.h"

#define MAX_PHYS   (1ULL << 30)
#define MAX_FRAMES (MAX_PHYS / PAGE_SIZE)

extern char _kernel_start[];
extern char _kernel_end[];

static uint8_t bitmap[MAX_FRAMES / 8]; // bit set = frame in use
static size_t total_frames = 0;
static size_t free_frames = 0;
static size_t search_hint = 0;

static int frame_used(size_t f) { return bitmap[f / 8] & (1 << (f % 8)); }
static void frame_set(size_t f) { bitmap[f / 8] |= (uint8_t) (1 << (f % 8)); }
static void frame_clear(size_t f) { bitmap[f / 8] &= (uint8_t) ~(1 << (f % 8)); }

static void release_range(uint64_t base, uint64_t length) {
    uint64_t end = base + length;
    if (end > MAX_PHYS) {
        end = MAX_PHYS;
    }
    for (uint64_t a = (base + PAGE_SIZE - 1) & ~(uint64_t) (PAGE_SIZE - 1); a + PAGE_SIZE <= end; a += PAGE_SIZE) {
        size_t f = a / PAGE_SIZE;
        if (frame_used(f)) {
            frame_clear(f);
            free_frames++;
            total_frames++;
        }
    }
}

static void reserve_range(uint64_t base, uint64_t length) {
    uint64_t end = base + length;
    if (end > MAX_PHYS) {
        end = MAX_PHYS;
    }
    for (uint64_t a = base & ~(uint64_t) (PAGE_SIZE - 1); a < end; a += PAGE_SIZE) {
        size_t f = a / PAGE_SIZE;
        if (!frame_used(f)) {
            frame_set(f);
            free_frames--;
        }
    }
}

void pmm_init(uint64_t mbi_addr) {
    memset(bitmap, 0xFF, sizeof(bitmap));

    struct mb2_info* info = (struct mb2_info*) mbi_addr;
    uint8_t* p = (uint8_t*) mbi_addr + sizeof(struct mb2_info);
    uint8_t* end = (uint8_t*) mbi_addr + info->total_size;
    int found = 0;

    while (p < end) {
        struct mb2_tag* tag = (struct mb2_tag*) p;
        if (tag->type == MB2_TAG_END) {
            break;
        }
        if (tag->type == MB2_TAG_MMAP) {
            struct mb2_mmap_tag* mmap = (struct mb2_mmap_tag*) tag;
            uint8_t* e = (uint8_t*) mmap + sizeof(*mmap);
            uint8_t* e_end = (uint8_t*) mmap + mmap->size;
            for (; e < e_end; e += mmap->entry_size) {
                struct mb2_mmap_entry* entry = (struct mb2_mmap_entry*) e;
                if (entry->type == MB2_MMAP_AVAILABLE) {
                    release_range(entry->base, entry->length);
                }
            }
            found = 1;
        }
        p += (tag->size + 7) & ~(uint32_t) 7; // tags are 8-byte aligned
    }

    if (!found) {
        kprintf("pmm: no memory map from bootloader!\n");
        return;
    }

    reserve_range(0, 0x100000); // BIOS area, VGA, and the real-mode leftovers
    reserve_range((uint64_t) _kernel_start, (uint64_t) (_kernel_end - _kernel_start));
    reserve_range(mbi_addr, info->total_size);
}

uint64_t pmm_alloc_contiguous(size_t frames) {
    if (frames == 0 || frames > free_frames) {
        return 0;
    }

    size_t run = 0;
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        size_t f = (search_hint + i) % MAX_FRAMES;
        if (f == 0) {
            run = 0; // a run must not wrap around the end of the bitmap
        }
        if (frame_used(f)) {
            run = 0;
            continue;
        }
        if (++run == frames) {
            size_t first = f + 1 - frames;
            for (size_t k = first; k <= f; k++) {
                frame_set(k);
            }
            free_frames -= frames;
            search_hint = (f + 1) % MAX_FRAMES;
            return (uint64_t) first * PAGE_SIZE;
        }
    }
    return 0;
}

uint64_t pmm_alloc_frame(void) {
    return pmm_alloc_contiguous(1);
}

void pmm_free_contiguous(uint64_t addr, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        size_t f = addr / PAGE_SIZE + i;
        if (f < MAX_FRAMES && frame_used(f)) {
            frame_clear(f);
            free_frames++;
        }
    }
}

void pmm_free_frame(uint64_t addr) {
    pmm_free_contiguous(addr, 1);
}

size_t pmm_total_frames(void) { return total_frames; }
size_t pmm_free_frames(void) { return free_frames; }
