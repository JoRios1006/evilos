#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Descriptor estándar de 64 bits (8 bytes)
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// Descriptor de sistema (TSS) en 64 bits (16 bytes)
struct tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

// Puntero para la instrucción lgdt
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Estructura de la TSS de 64 bits
struct tss_64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7]; // Interrupt Stack Table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

void gdt_init(void);

#endif
