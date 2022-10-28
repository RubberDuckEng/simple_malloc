#pragma once

#include <stdint.h>

void init_heap(void);

void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t number_of_members, size_t size);
void *my_realloc(void *ptr, size_t size);

void dump_heap(void);
