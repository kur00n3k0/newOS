#pragma once

#include <stdint.h>

#define MB2_TAG_END    0
#define MB2_TAG_MMAP   6

#define MB2_MMAP_AVAILABLE 1

struct mb2_info {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed));

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_mmap_tag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed));

struct mb2_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));
