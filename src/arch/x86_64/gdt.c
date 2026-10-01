#include "gdt.h"
#include <stddef.h> // Para NULL (si hace falta)
#include <stdint.h>

static struct gdt_entry gdt[7];
static struct gdt_ptr gdtr;
static struct tss_64 tss;

// Stack de emergencia exclusivo para el Double Fault (16 KiB)
static uint8_t double_fault_stack[16384] __attribute__((aligned(16)));

extern void gdt_flush(uint64_t gdtr_ptr);

static void gdt_set_entry(int index, uint8_t access, uint8_t gran) {
    gdt[index].limit_low = 0xFFFF;
    gdt[index].base_low = 0;
    gdt[index].base_middle = 0;
    gdt[index].access = access;
    gdt[index].granularity = gran | 0x0F;
    gdt[index].base_high = 0;
}

int gdt_init(void) {
    // 0: Nulo
    gdt_set_entry(0, 0, 0);
    // 1: Código Kernel
    gdt_set_entry(1, 0x9A, 0xA0);
    // 2: Datos Kernel
    gdt_set_entry(2, 0x92, 0xC0);
    // 3: Datos Usuario
    gdt_set_entry(3, 0xF2, 0xC0);
    // 4: Código Usuario
    gdt_set_entry(4, 0xFA, 0xA0);

    // --- CONFIGURACIÓN DE LA TSS ---
    
    // Limpiar la estructura TSS (similar a memset)
    uint8_t *tss_ptr = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(struct tss_64); i++) tss_ptr[i] = 0;

    // Asignar el stack de emergencia al IST 1
    // (El stack crece hacia abajo, así que apuntamos al FINAL del arreglo)
    tss.ist[0] = (uint64_t)double_fault_stack + sizeof(double_fault_stack);

    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_64) - 1;

    // 5: Mitad inferior del descriptor TSS (8 bytes)
    gdt[5].limit_low = tss_limit & 0xFFFF;
    gdt[5].base_low = tss_base & 0xFFFF;
    gdt[5].base_middle = (tss_base >> 16) & 0xFF;
    gdt[5].access = 0x89; // Presente, Ring 0, Tipo TSS 64-bit
    gdt[5].granularity = ((tss_limit >> 16) & 0x0F) | 0x00;
    gdt[5].base_high = (tss_base >> 24) & 0xFF;

    // 6: Mitad superior del descriptor TSS (8 bytes)
    gdt[6].limit_low = (tss_base >> 32) & 0xFFFF;
    gdt[6].base_low = (tss_base >> 48) & 0xFFFF;
    gdt[6].base_middle = 0;
    gdt[6].access = 0;
    gdt[6].granularity = 0;
    gdt[6].base_high = 0;

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt[0];

    gdt_flush((uint64_t)&gdtr);

    // Cargar el Task Register (TR) con el selector de la TSS (índice 5 -> 0x28)
    __asm__ volatile ("ltr %0" : : "r" ((uint16_t)0x28));
	return 1;
}
