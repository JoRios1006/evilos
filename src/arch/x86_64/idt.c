#include "idt.h"

static struct idt_entry idt[256];
static struct idtr idtr;

static void idt_set_entry(int vector, void (*isr)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08; // El selector de código de tu GDT
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

// Declarar las 32 rutinas de ensamblador con prototipo explícito (void)
extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

// Arreglo de punteros a funciones (evita el error de conversión de void*)
static void (*isr_stubs[32])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9, isr10, isr11,
    isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19, isr20, isr21, isr22,
    isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

void idt_init(void) {
    // Inicializar las 32 excepciones de CPU
    for (int i = 0; i < 32; i++) {
        idt_set_entry(i, isr_stubs[i], 0x8E); // 0x8E: Presente, Ring 0, Interrupt Gate
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt[0];

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
