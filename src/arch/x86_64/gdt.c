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
    gdt[index].limit_low = 0xFFFFU;
    gdt[index].base_low = 0U;
    gdt[index].base_middle = 0U;
    gdt[index].access = access;
    gdt[index].granularity = gran | 0x0FU;
    gdt[index].base_high = 0U;
}

int gdt_init(void) {
    // 0: Nulo
    gdt_set_entry(0, 0U, 0U);
    // 1: Código Kernel
    gdt_set_entry(1, 0x9AU, 0xA0U);
    // 2: Datos Kernel
    gdt_set_entry(2, 0x92U, 0xC0U);
    // 3: Datos Usuario
    gdt_set_entry(3, 0xF2U, 0xC0U);
    // 4: Código Usuario
    gdt_set_entry(4, 0xFAU, 0xA0U);

    // --- CONFIGURACIÓN DE LA TSS ---
    
    // Limpiar la estructura TSS (similar a memset)
    uint8_t *tss_ptr = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(struct tss_64); i++) {
        tss_ptr[i] = 0U;
    }

    // Asignar el stack de emergencia al IST 1
    // (El stack crece hacia abajo, así que apuntamos al FINAL del arreglo)
    // Caseteo doble a través de uintptr_t (MISRA 11.4)
    // cppcheck-suppress misra-c2012-11.4
    tss.ist[0] = (uint64_t)double_fault_stack + sizeof(double_fault_stack);

    // cppcheck-suppress misra-c2012-11.4
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_64) - 1U; // 1U para que coincida con size_t (MISRA 10.4)

    // 5: Mitad inferior del descriptor TSS (8 bytes)
    gdt[5].limit_low = tss_limit & 0xFFFFU;
    gdt[5].base_low = tss_base & 0xFFFFU;
    gdt[5].base_middle = (tss_base >> 16) & 0xFFU;
    gdt[5].access = 0x89U; // Presente, Ring 0, Tipo TSS 64-bit
	gdt[5].granularity = ((tss_limit >> 16) & 0x0FU);
    gdt[5].base_high = (tss_base >> 24) & 0xFFU;

    // 6: Mitad superior del descriptor TSS (8 bytes)
    gdt[6].limit_low = (tss_base >> 32) & 0xFFFFU;
    gdt[6].base_low = (tss_base >> 48) & 0xFFFFU;
    gdt[6].base_middle = 0U;
    gdt[6].access = 0U;
    gdt[6].granularity = 0U;
    gdt[6].base_high = 0U;

    gdtr.limit = sizeof(gdt) - 1U;
    // cppcheck-suppress misra-c2012-11.4
    gdtr.base = (uint64_t)(uintptr_t)&gdt[0];

    // cppcheck-suppress misra-c2012-11.4
    gdt_flush((uint64_t)(uintptr_t)&gdtr);

    // Cargar el Task Register (TR) con el selector de la TSS (índice 5 -> 0x28)
    __asm__ volatile ("ltr %0" : : "r" ((uint16_t)0x28U));
    
    return 1;
}
