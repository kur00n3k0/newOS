#pragma once

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

// Only the first 1 GiB is tracked: that is what the boot code identity-maps.
void pmm_init(uint64_t mbi_addr);
uint64_t pmm_alloc_frame(void);                 // returns 0 on failure
uint64_t pmm_alloc_contiguous(size_t frames);   // returns 0 on failure
void pmm_free_frame(uint64_t addr);
void pmm_free_contiguous(uint64_t addr, size_t frames);
size_t pmm_total_frames(void);
size_t pmm_free_frames(void);
