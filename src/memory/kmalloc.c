#include "kmalloc.h"
#include "../limine.h"
#include "string.h"

// Definir BUDDY_PRINTF antes de incluir la cabecera para evitar que cargue
// <stdio.h> del host
extern void kprintf(const char *format, ...);
#define BUDDY_PRINTF kprintf
#include "buddy_alloc.h"

extern volatile struct limine_memmap_request memmap_req;
extern volatile struct limine_hhdm_request hhdm_req;

static struct buddy *kernel_buddy = NULL;

void kmalloc_init(void) {
  if (memmap_req.response == NULL || hhdm_req.response == NULL)
    return;

  uint64_t hhdm_offset = hhdm_req.response->offset;
  struct limine_memmap_entry *largest_entry = NULL;
  uint64_t max_size = 0;

  for (uint64_t i = 0; i < memmap_req.response->entry_count; i++) {
    struct limine_memmap_entry *entry = memmap_req.response->entries[i];
    if (entry->type == LIMINE_MEMMAP_USABLE && entry->length > max_size) {
      max_size = entry->length;
      largest_entry = entry;
    }
  }

  if (largest_entry == NULL)
    return;

  void *arena_virtual = (void *)(largest_entry->base + hhdm_offset);
  kernel_buddy = buddy_embed(arena_virtual, largest_entry->length);

  if (kernel_buddy) {
    kprintf("[OK] kmalloc inicializado en 0x%x (Tam: %d MB)\n",
            (uint64_t)arena_virtual, largest_entry->length / (1024 * 1024));
  }
  if (largest_entry->length < (16 * 1024)) {
    kprintf("[PANIC] Region usable demasiado pequena para el allocator.\n");
    __asm__ volatile("cli; hlt");
  }

  // VALIDACIÓN CRÍTICA
  if (!kernel_buddy) {
    kprintf("[PANIC] buddy_embed fallo al inicializar la arena.\n");
    __asm__ volatile("cli; hlt");
  }

  kprintf("[OK] kmalloc inicializado en 0x%x (Tam: %d MB)\n",
          (uint64_t)arena_virtual, largest_entry->length / (1024 * 1024));
}

void *kmalloc(size_t size) {
  if (!kernel_buddy)
    return NULL;
  return buddy_malloc(kernel_buddy, size);
}

void kfree(void *ptr) {
  if (!kernel_buddy || !ptr)
    return;
  buddy_free(kernel_buddy, ptr);
}
