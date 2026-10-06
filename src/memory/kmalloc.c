#include "kmalloc.h"
#include "../limine.h"
#include "string.h"

extern void kprintf(const char *format, ...);
#define BUDDY_PRINTF kprintf
#include "buddy_alloc.h"

extern volatile struct limine_memmap_request memmap_req;
extern volatile struct limine_hhdm_request hhdm_req;

struct buddy *kmalloc_init(void) {
  if (memmap_req.response == NULL || hhdm_req.response == NULL)
    return NULL;

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

  if (largest_entry == NULL) {
    return NULL;
  }

  void *arena_virtual = (void *)(largest_entry->base + hhdm_offset);

  // Create the buddy instance
  struct buddy *kb = buddy_embed(arena_virtual, largest_entry->length);

  if (kb != NULL) {
    kprintf("[OK] kmalloc inicializado en 0x%x (Tam: %d MB)\n",
            (uint64_t)arena_virtual, largest_entry->length / (1024 * 1024));
  }
  if (largest_entry->length < (16 * 1024)) {
    kprintf("[PANIC] Region usable demasiado pequena para el allocator.\n");
    __asm__ volatile("cli; hlt");
  }

  if (kb == NULL) {
    kprintf("[PANIC] buddy_embed fallo al inicializar la arena.\n");
    __asm__ volatile("cli; hlt");
  }

  return kb; // Return it to kmain
}

void *kmalloc(struct buddy *kernel_buddy, size_t size) {
  if (kernel_buddy == NULL) {
    return NULL;
  }
  return buddy_malloc(kernel_buddy, size);
}

void kfree(struct buddy *kernel_buddy, void *ptr) {
  if ((kernel_buddy == NULL) || (ptr == NULL)) {
    return;
  }
  buddy_free(kernel_buddy, ptr);
}
