#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Entrada de 64 bits de la IDT (16 bytes)
struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// El estado exacto del CPU en el momento del fallo
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss; // Apilados automáticamente por el CPU
} __attribute__((packed));

int idt_init(void);

#endif
