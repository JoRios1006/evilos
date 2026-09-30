#include "idt.h"

// Funciones UART externas para volcar el diagnóstico si ocurre un pánico
extern void uart_puts(const char *str);
extern void uart_print_hex(uint64_t value);

// Nombres descriptivos para las 32 excepciones estándar de la arquitectura x86-64
static const char *exception_messages[] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPT Floating-Point Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

// Esta es la función que llama isr_stubs.S
void isr_handler(struct interrupt_frame *frame) {
    uart_puts("\n\n========================================\n");
    uart_puts("       KERNEL PANIC: EXCEPCION DE CPU    \n");
    uart_puts("========================================\n");
    
    uart_puts("Excepcion: ");
    if (frame->int_number < 32) {
        uart_puts(exception_messages[frame->int_number]);
    } else {
        uart_puts("Desconocida");
    }
    uart_puts(" (Vector: ");
    uart_print_hex(frame->int_number);
    uart_puts(")\n");

    uart_puts("Codigo de Error: ");
    uart_print_hex(frame->error_code);
    uart_puts("\n\n");

    // Volcado de registros críticos
    uart_puts("RIP: "); uart_print_hex(frame->rip); uart_puts("\n");
    uart_puts("RSP: "); uart_print_hex(frame->rsp); uart_puts("\n");
    uart_puts("RAX: "); uart_print_hex(frame->rax); uart_puts(" | RBX: "); uart_print_hex(frame->rbx); uart_puts("\n");
    uart_puts("RCX: "); uart_print_hex(frame->rcx); uart_puts(" | RDX: "); uart_print_hex(frame->rdx); uart_puts("\n");

    uart_puts("\n[KERNEL] Sistema detenido de forma controlada.\n");
    
    // Bucle infinito de seguridad para congelar el CPU
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
