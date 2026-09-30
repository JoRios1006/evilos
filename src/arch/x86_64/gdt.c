#include "gdt.h"

// 5 descriptores estándar + 1 descriptor TSS que ocupa el espacio de 2 en x86_64
static struct gdt_entry gdt[7];
static struct gdt_ptr gdtr;
//static struct tss_64 tss;

// Funciones ensamblador externas para recargar registros
extern void gdt_flush(uint64_t gdtr_ptr);
extern void tss_flush(void);

static void gdt_set_entry(int index, uint8_t access, uint8_t gran) {
    gdt[index].limit_low = 0xFFFF;
    gdt[index].base_low = 0;
    gdt[index].base_middle = 0;
    gdt[index].access = access;
    gdt[index].granularity = gran | 0x0F;
    gdt[index].base_high = 0;
}

void gdt_init(void) {
    // 0: Descriptor nulo
    gdt_set_entry(0, 0, 0);

    // 1: Código Kernel (Ring 0)
    gdt_set_entry(1, 0x9A, 0xA0); // 0x9A: Presente, Ejecutable, Leíble. 0xA0: 64-bit flag

    // 2: Datos Kernel (Ring 0)
    gdt_set_entry(2, 0x92, 0xC0); // 0x92: Presente, Escribible. 0xC0: 32-bit (protección ignorada en 64-bit)

    // 3: Datos Usuario (Ring 3)
    gdt_set_entry(3, 0xF2, 0xC0); // 0xF2: Ring 3

    // 4: Código Usuario (Ring 3)
    gdt_set_entry(4, 0xFA, 0xA0); // 0xFA: Ring 3, 64-bit

    // Configurar la TSS

    // Configurar el puntero de la GDT
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt[0];

    // Cargar la nueva GDT
    gdt_flush((uint64_t)&gdtr);
}
