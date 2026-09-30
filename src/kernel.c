#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch/x86_64/idt.h"
#include "limine.h"
#include <float.h>
#include <flanterm.h>
#include <flanterm_backends/fb.h>
#include "arch/x86_64/gdt.h"
#include "drivers/uart.h"
// 1. Marcador de inicio
__attribute__((used, section(".requests_start")))
static volatile uint64_t start_marker[4] = LIMINE_REQUESTS_START_MARKER;

// 2. Revisión base del protocolo
__attribute__((used, section(".requests")))
static volatile uint64_t base_revision[3] = LIMINE_BASE_REVISION(3);

// 3. Solicitudes anteriores (HHDM y Framebuffer)
__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// 4. NUEVAS SOLICITUDES PARA TUS TAREAS
__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_executable_address_request kernel_addr_req = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_rsdp_request rsdp_req = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

// 5. Marcador de fin
__attribute__((used, section(".requests_end")))
static volatile uint64_t end_marker[2] = LIMINE_REQUESTS_END_MARKER;

// Cadenas descriptivas para los tipos de memoria de Limine
static const char *memmap_type_strings[] = {
    "USABLE", "RESERVED", "ACPI_RECLAIMABLE", "ACPI_NVS", 
    "BAD_MEMORY", "BOOTLOADER_RECLAIMABLE", "EXECUTABLE_AND_MODULES", 
    "FRAMEBUFFER", "RESERVED_MAPPED"
};

// Variables para verificar la inicialización de C (Entrada)
unsigned int variable_global_inicializada = 0xCAFEBABE;
int variable_bss_cero; // Debe inicializarse automáticamente en 0

void kmain(void) {
    uart_init();
    gdt_init();
    idt_init();
    uart_puts("\n\n=== REPORTE DE SISTEMA ===\n");

    // Verificar Globales y BSS
    if (variable_global_inicializada == 0xCAFEBABE && variable_bss_cero == 0) {
        uart_puts("[OK] C Runtime: Globales y BSS inicializados correctamente.\n");
    }

    // Obtener RSDP
    if (rsdp_req.response != NULL) {
        uart_puts("[INFO] ACPI RSDP encontrado en: ");
        uart_print_hex((uint64_t)rsdp_req.response->address);
        uart_puts("\n");
    }

    // Identificar la región ocupada por el kernel
    if (kernel_addr_req.response != NULL) {
        uart_puts("[INFO] Kernel Base Fisica:  ");
        uart_print_hex(kernel_addr_req.response->physical_base);
        uart_puts("\n[INFO] Kernel Base Virtual: ");
        uart_print_hex(kernel_addr_req.response->virtual_base);
        uart_puts("\n");
    }

    // Verificar HHDM
    if (hhdm_req.response != NULL) {
        uart_puts("[INFO] HHDM Offset: ");
        uart_print_hex(hhdm_req.response->offset);
        uart_puts("\n");
    } else {
        uart_puts("[ERROR] HHDM no disponible.\n");
    }

    // Inicializar Flanterm y renderizar el entorno gráfico
    if (fb_req.response != NULL && fb_req.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
        
        struct flanterm_context *ft_ctx = flanterm_fb_init(
            NULL, NULL, 
            fb->address, fb->width, fb->height, fb->pitch,
            fb->red_mask_size, fb->red_mask_shift,
            fb->green_mask_size, fb->green_mask_shift,
            fb->blue_mask_size, fb->blue_mask_shift,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
            0, 0, 1, 0, 0, 0, 0, true
        );

        const char msg[] = "\n\033[32m[OK]\033[0m Emulador de terminal VT100 inicializado.\n"
                           "\n\033[36mBienvenidos a Evilos (x86_64)\033[0m\n\n";
        flanterm_write(ft_ctx, msg, sizeof(msg) - 1);
        
        uart_puts("[INFO] Flanterm instanciado en el Framebuffer primario.\n");
    } else {
        uart_puts("[WARNING] Framebuffer no disponible.\n");
    }

    // Leer memory map e imprimir todas sus entradas
    if (memmap_req.response != NULL) {
        uart_puts("\n--- MAPA DE MEMORIA FISICA ---\n");
        uint64_t count = memmap_req.response->entry_count;
        
        for (uint64_t i = 0; i < count; i++) {
            struct limine_memmap_entry *entry = memmap_req.response->entries[i];
            
            uart_puts("Base: ");
            uart_print_hex(entry->base);
            uart_puts(" | Size: ");
            uart_print_hex(entry->length);
            
            uart_puts(" | Tipo: ");
            if (entry->type <= LIMINE_MEMMAP_RESERVED_MAPPED) {
                uart_puts(memmap_type_strings[entry->type]);
            } else {
                uart_puts("UNKNOWN");
            }
            uart_puts("\n");
        }
        uart_puts("------------------------------\n");
    }

    uart_puts("\n[KERNEL] Halt.\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
