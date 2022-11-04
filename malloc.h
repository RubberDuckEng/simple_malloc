#pragma once

#include <stdint.h>

void init_heap(void);

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t number_of_members, size_t size);
void *realloc(void *ptr, size_t size);

void dump_heap(void);
