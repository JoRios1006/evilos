#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

struct buddy;

struct buddy *kmalloc_init(void);
void *kmalloc(struct buddy *kernel_buddy, size_t size);
void kfree(struct buddy *kernel_buddy, void *ptr);

#endif
