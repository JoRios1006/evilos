#include "uart.h"
#include <stdint.h>

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

int uart_init(uint16_t port) {
    outb(port + 1, 0x00);
    outb(port + 3, 0x80);
    outb(port + 0, 0x03);
    outb(port + 1, 0x00);
    outb(port + 3, 0x03);
    outb(port + 2, 0xC7);
    outb(port + 4, 0x0B);
	return 1;
}

void uart_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

void uart_puts(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        uart_putc(str[i]);
    }
}

void uart_print_hex(uint64_t value) {
    const char *hex_chars = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 15; i >= 0; i--) {
        uart_putc(hex_chars[(value >> (i * 4)) & 0xF]);
    }
}
