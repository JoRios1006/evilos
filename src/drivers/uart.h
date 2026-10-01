#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

int uart_init(uint16_t port);
void uart_putc(char c);
void uart_puts(const char *str);
void uart_print_hex(uint64_t value);

#endif
