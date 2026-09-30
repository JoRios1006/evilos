.intel_syntax noprefix

.extern isr_handler

# Macro para excepciones SIN código de error
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    push 0          # Código de error ficticio
    push \num       # Número de interrupción
    jmp isr_common
.endm

# Macro para excepciones CON código de error
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    # El CPU ya empujó el código de error
    push \num
    jmp isr_common
.endm

# Definir las 32 excepciones de CPU
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8  # Double Fault
ISR_NOERRCODE 9
ISR_ERRCODE   10 # Invalid TSS
ISR_ERRCODE   11 # Segment Not Present
ISR_ERRCODE   12 # Stack-Segment Fault
ISR_ERRCODE   13 # General Protection Fault
ISR_ERRCODE   14 # Page Fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

isr_common:
    # Guardar todos los registros de propósito general
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    # Pasar el puntero del stack a la función en C (Argumento 1: RDI)
    mov rdi, rsp
    
    # Llamar al manejador en C
    call isr_handler

    # Restaurar los registros
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    # Limpiar el número de interrupción y el código de error del stack (2 * 8 bytes)
    add rsp, 16
    
    # Retorno de interrupción
    iretq
