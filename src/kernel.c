#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

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

// --- Funciones UART ---
static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void uart_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static void uart_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

static void uart_puts(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        uart_putc(str[i]);
    }
}

// Función auxiliar para imprimir números hexadecimales (imprescindible para direcciones de memoria)
static void uart_print_hex(uint64_t value) {
    const char *hex_chars = "0123456789ABCDEF";
    uart_puts("0x");
    // Imprimir los 16 nibbles (64 bits)
    for (int i = 15; i >= 0; i--) {
        uart_putc(hex_chars[(value >> (i * 4)) & 0xF]);
    }
}

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

    // Verificar HHDM (Crítico para el PMM)
    if (hhdm_req.response != NULL) {
        uart_puts("[INFO] HHDM Offset: ");
        uart_print_hex(hhdm_req.response->offset);
        uart_puts("\n");
    } else {
        uart_puts("[ERROR] HHDM no disponible.\n");
    }

    // Identificar framebuffer y registrar detalles técnicos
    if (fb_req.response != NULL && fb_req.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

        uart_puts("[INFO] Framebuffer Address: ");
        uart_print_hex((uint64_t)fb->address);

        uart_puts("\n[INFO] Framebuffer Pitch: ");
        uart_print_hex(fb->pitch);

        uart_puts("\n[INFO] Framebuffer Size: ");
        uart_print_hex(fb->width);
        uart_puts("x");
        uart_print_hex(fb->height);

        uart_puts("\n[INFO] Framebuffer BPP: ");
        uart_print_hex(fb->bpp);
        
        uart_puts("\n[INFO] Framebuffer Memory Model: ");
        uart_print_hex(fb->memory_model);
        uart_puts("\n");
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
