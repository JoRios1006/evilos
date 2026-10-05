#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "drivers/uart.h"
#include "limine.h"
#include "string.h"
#include <flanterm.h>
#include <flanterm_backends/fb.h>
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "memory/kmalloc.h"
#define NUKE *(volatile char *)0 = 0;
#define UART_PORT 0x3F8
#define ON_SUCCESS(expr, msg)                                                  \
  do {                                                                         \
    if (expr != 0) {                                                           \
      kprintf(msg);                                                            \
    } else {                                                                   \
      goto HALT;                                                               \
    }                                                                          \
  } while (0)
#define ON_ERROR(msg)                                                          \
  do {                                                                         \
    kprintf(msg);                                                              \
    goto HALT;                                                                 \
  } while (0)
// 1. Marcador de inicio
__attribute__((
    used,
    section(".requests_start"))) static volatile uint64_t start_marker[4] =
    LIMINE_REQUESTS_START_MARKER;

// 2. Revisión base del protocolo
__attribute__((
    used, section(".requests"))) static volatile uint64_t base_revision[3] =
    LIMINE_BASE_REVISION(3);

// 3. Solicitudes anteriores (HHDM y Framebuffer)
__attribute__((used,
               section(".requests"))) volatile struct limine_hhdm_request
    hhdm_req = {.id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

__attribute__((
    used,
    section(".requests"))) volatile struct limine_framebuffer_request
    fb_req = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

// 4. NUEVAS SOLICITUDES PARA TAREAS
__attribute__((
    used, section(".requests"))) volatile struct limine_memmap_request
    memmap_req = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

__attribute__((
    used,
    section(
        ".requests"))) static volatile struct limine_executable_address_request
    kernel_addr_req = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
                       .revision = 0};

__attribute__((used,
               section(".requests"))) static volatile struct limine_rsdp_request
    rsdp_req = {.id = LIMINE_RSDP_REQUEST_ID, .revision = 0};

// 5. Marcador de fin
__attribute__((
    used, section(".requests_end"))) static volatile uint64_t end_marker[2] =
    LIMINE_REQUESTS_END_MARKER;

// Contexto global de la terminal para poder usarlo desde cualquier parte
struct flanterm_context *global_ft_ctx = NULL;

// kprintf escribe en el puerto serie y (si está lista) en la pantalla gráfica
void kprintf(const char *format, ...) {
  char buf[512];
  va_list args;

  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  uart_puts(buf); // El UART del host Linux ya maneja \n correctamente

  if (global_ft_ctx != NULL) {
    // Inyectar explícitamente \r antes de cada \n para el estándar VT100
    for (size_t i = 0; buf[i] != '\0'; i++) {
      if (buf[i] == '\n') {
        flanterm_write(global_ft_ctx, "\r", 1);
      }
      flanterm_write(global_ft_ctx, &buf[i], 1);
    }
  }
}
void test_kmalloc_stress(void) {
    kprintf("[INFO] Iniciando bateria de pruebas de memoria...\n");

    // 1. Prueba de alineación y bordes
    uint8_t *p1 = kmalloc(1);
    uint8_t *p2 = kmalloc(4096);
    if ((uintptr_t)p1 % 8 != 0 || (uintptr_t)p2 % 8 != 0) {
        kprintf("[PANIC] Error de alineacion en kmalloc.\n");
        __asm__ volatile("cli; hlt");
    }

    // 2. Prueba de Coalescing (fusión de bloques)
    kfree(p1);
    uint8_t *p3 = kmalloc(1); 
    // Si el coalescing y re-alloc funcionan, p3 debería ocupar el lugar de p1
    
    // 3. Prueba de carga masiva
    void *stress_array[100];
    for (int i = 0; i < 100; i++) {
        stress_array[i] = kmalloc(1024);
        if (!stress_array[i]) {
            kprintf("[PANIC] kmalloc fallo en iteracion %d.\n", i);
            __asm__ volatile("cli; hlt");
        }
    }

    // Liberación masiva
    for (int i = 0; i < 100; i++) {
        kfree(stress_array[i]);
    }
    
    kfree(p2);
    kfree(p3);
    
    // Este es el string que tu Lua Test Runner debe buscar ahora
    kprintf("[OK] Pruebas de estres de memoria superadas.\n");
}

// Cadenas descriptivas para los tipos de memoria de Limine
static const char *memmap_type_strings[] = {"USABLE",
                                            "RESERVED",
                                            "ACPI_RECLAIMABLE",
                                            "ACPI_NVS",
                                            "BAD_MEMORY",
                                            "BOOTLOADER_RECLAIMABLE",
                                            "EXECUTABLE_AND_MODULES",
                                            "FRAMEBUFFER",
                                            "RESERVED_MAPPED"};

// Variables para verificar la inicialización de C (Entrada)
unsigned int vcanary = 0xCAFEBABE;
uint8_t gi; // Debe inicializarse automáticamente en 0
static const char vterm_msg[] =
    "\n\033[32m[OK]\033[0m Emulador de terminal VT100 inicializado.\n"
    "\n\033[36mBienvenidos a Evilos (x86_64)\033[0m\n\n";

void kmain(void) {
  // INITIALIZE_FLANTERM:;
  if (fb_req.response == NULL || fb_req.response->framebuffer_count == 0)
    goto WARNING_NOFB;
  struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
  struct flanterm_context *ft_ctx = flanterm_fb_init(
      NULL, NULL, fb->address, fb->width, fb->height, fb->pitch,
      fb->red_mask_size, fb->red_mask_shift, fb->green_mask_size,
      fb->green_mask_shift, fb->blue_mask_size, fb->blue_mask_shift, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, true);
  global_ft_ctx = ft_ctx;
  kprintf(vterm_msg);
  kprintf("[INFO] Flanterm instanciado en el Framebuffer primario.\n");
  goto ESSENTIAL_INIT;
WARNING_NOFB:;
  kprintf("[WARNING] Framebuffer no disponible.\n");
ESSENTIAL_INIT:;
  ON_SUCCESS(uart_init(UART_PORT), "\n[OK] UART DEVICE INITIALIZED");
  ON_SUCCESS(gdt_init(), "\n[OK] GDT INITIALIZED");
  ON_SUCCESS(idt_init(), "\n[OK] IDT INITIALIZED");
  kmalloc_init();
  test_kmalloc_stress();
  // Verificar Globales y BSS
  ON_SUCCESS(vcanary == 0xCAFEBABE && gi == 0,
             "\n[OK] C Runtime: Globales y BSS inicializados correctamente.\n");

  // Obtener RSDP
  if (rsdp_req.response == NULL)
    goto ERROR_RSDP;
  kprintf("[INFO] ACPI RSDP encontrado en: ");
  uart_print_hex((uint64_t)rsdp_req.response->address);
  kprintf("\n");
  goto IDENTIFY_REGION;
ERROR_RSDP:
  ON_ERROR("[INFO]?1");

IDENTIFY_REGION:;
  if (kernel_addr_req.response == NULL)
    goto ERROR_BASE;
  kprintf("[INFO] Kernel Base Fisica:  ");
  uart_print_hex(kernel_addr_req.response->physical_base);
  kprintf("\n[INFO] Kernel Base Virtual: ");
  uart_print_hex(kernel_addr_req.response->virtual_base);
  kprintf("\n");
  goto VERIFY_HDDM;
ERROR_BASE:;
  ON_ERROR("[INFO]?1");

VERIFY_HDDM:;
  if (hhdm_req.response == NULL)
    goto ERROR_HHDM;
  kprintf("[INFO] HHDM Offset: ");
  uart_print_hex(hhdm_req.response->offset);
  kprintf("\n");
  goto READ_MAP;
ERROR_HHDM:;
  ON_ERROR("[ERROR] HHDM no disponible.\n");

READ_MAP:;
  if (memmap_req.response == NULL)
    goto HALT;
  uint64_t count = memmap_req.response->entry_count;
  kprintf("\n--- MAPA DE MEMORIA FISICA ---\n");

SHOW_MEMORY:;
  struct limine_memmap_entry *entry = memmap_req.response->entries[gi];
  const char *type_str = (entry->type <= LIMINE_MEMMAP_RESERVED_MAPPED)
                             ? memmap_type_strings[entry->type]
                             : "UNKNOWN";
  kprintf("Base: 0x%x | Size: 0x%x | Tipo: %s\n", entry->base, entry->length,
          type_str);
  gi++;
  if (gi < count)
    goto SHOW_MEMORY;
  gi = 0;
  // Inicializar memoria dinámica
  kmalloc_init();

  // Prueba de humo de la Fase 3
  void *test_ptr = kmalloc(4096);
  if (test_ptr) {
    kprintf("[OK] Prueba de kmalloc exitosa. Bloque asignado en: 0x%x\n",
            (uint64_t)test_ptr);
    kfree(test_ptr);
    kprintf("[OK] kfree ejecutado correctamente.\n");
  } else {
    kprintf("[ERROR] Fallo en la prueba de kmalloc.\n");
  }
HALT:
  kprintf("\n[KERNEL] Halt.\n");
HALT2:;
  __asm__ volatile("hlt");
  goto HALT2;
}
