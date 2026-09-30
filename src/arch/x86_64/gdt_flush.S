.intel_syntax noprefix

.global gdt_flush
gdt_flush:
    # RDI contiene el puntero a gdtr pasado desde C
    lgdt [rdi]

    # Recargar segmentos de datos con el selector 0x10 (Data Kernel)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    # Recargar CS (Selector 0x08 - Code Kernel) mediante un salto lejano
    push 0x08
    lea rax, [rip + .reload_cs]
    push rax
    retfq

.reload_cs:
    ret
