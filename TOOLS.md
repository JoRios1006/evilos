* **Toolchain** Clang + LLVM lld + NASM (inline assembly preferible).
* **Kernel Layout** Higher-Half Kernel (-mcmodel=kernel) binario ELF64.
* **Bootloader** Limine con solicitudes de HHDM, Memory Map y Framebuffer.
* **Debug** Puerto serie COM1 (-serial stdio en QEMU) + pantalla de Panic con volcado de registros.
* **Build** CMake + Ninja + ejecutable de QEMU 
