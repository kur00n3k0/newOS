#pragma once

#include <stddef.h>

void heap_init(void);
void* kmalloc(size_t size);
void* kcalloc(size_t count, size_t size);
void kfree(void* ptr);
size_t heap_used(void);
size_t heap_free(void);
