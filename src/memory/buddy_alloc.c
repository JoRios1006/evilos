// Declarar kprintf para satisfacer las normas estrictas de compilación
extern void kprintf(const char *format, ...);
#define BUDDY_PRINTF kprintf

// Único punto donde se genera la implementación de la librería
#define BUDDY_ALLOC_IMPLEMENTATION
#include "buddy_alloc.h"
