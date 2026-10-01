cmake_minimum_required(VERSION 3.20)
project(osdev_kernel C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_ASM_COMPILER "clang") 
set(CMAKE_LINKER "ld.lld")

# 1. Módulo para descargar automáticamente las herramientas de Limine desde GitHub
include(FetchContent)
FetchContent_Declare(
    limine
    GIT_REPOSITORY https://github.com/limine-bootloader/limine.git
    GIT_TAG        v11.x-binary
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(limine)

FetchContent_Declare(
    flanterm
    GIT_REPOSITORY https://github.com/Mintsuki/Flanterm.git
    GIT_TAG        trunk
)
FetchContent_MakeAvailable(flanterm)
set(TARGET_TRIPLE "x86_64-unknown-none-elf")

set(KERNEL_CFLAGS
    --target=${TARGET_TRIPLE}
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-stack-check
    -fno-lto
    -fno-PIE
    -mno-red-zone
    -mno-mmx
    -mno-sse
    -mno-sse2
    -mcmodel=kernel
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O2
    -g
)

set(KERNEL_LDFLAGS
    --target=${TARGET_TRIPLE}
    -fuse-ld=lld
    -T ${CMAKE_SOURCE_DIR}/linker.ld
    -nostdlib
    -static
    -Wl,--build-id=none
    -no-pie
)

# Modificar el ejecutable para incluir las fuentes de Flanterm
add_executable(kernel.elf
    src/kernel.c
    src/libk.c
    src/drivers/uart.c
    src/arch/x86_64/gdt.c
    src/arch/x86_64/gdt_flush.S
    src/arch/x86_64/idt.c
    src/arch/x86_64/isr_stubs.S
    src/arch/x86_64/isr.c
    ${flanterm_SOURCE_DIR}/src/flanterm.c
    ${flanterm_SOURCE_DIR}/src/flanterm_backends/fb.c
)
target_include_directories(kernel.elf PRIVATE ${flanterm_SOURCE_DIR}/src)
target_compile_options(kernel.elf PRIVATE ${KERNEL_CFLAGS})
target_link_options(kernel.elf PRIVATE ${KERNEL_LDFLAGS})

# 2. Generación de ISO utilizando la ruta descargada (${limine_SOURCE_DIR})
add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso_root/boot/limine
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/kernel.elf ${CMAKE_BINARY_DIR}/iso_root/boot/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/limine.conf ${CMAKE_BINARY_DIR}/iso_root/boot/limine/
    COMMAND ${CMAKE_COMMAND} -E copy ${limine_SOURCE_DIR}/limine-bios.sys ${CMAKE_BINARY_DIR}/iso_root/boot/limine/
    COMMAND ${CMAKE_COMMAND} -E copy ${limine_SOURCE_DIR}/limine-bios-cd.bin ${CMAKE_BINARY_DIR}/iso_root/boot/limine/
    COMMAND ${CMAKE_COMMAND} -E copy ${limine_SOURCE_DIR}/limine-uefi-cd.bin ${CMAKE_BINARY_DIR}/iso_root/boot/limine/
    COMMAND xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin
            -no-emul-boot -boot-load-size 4 -boot-info-table
            --efi-boot boot/limine/limine-uefi-cd.bin
            -efi-boot-part --efi-boot-image --protective-msdos-label
            ${CMAKE_BINARY_DIR}/iso_root -o ${CMAKE_BINARY_DIR}/kernel.iso
#    COMMAND ${limine_SOURCE_DIR}/limine bios-install ${CMAKE_BINARY_DIR}/kernel.iso
    DEPENDS kernel.elf
    COMMENT "Descargando e integrando Limine para generar la ISO..."
)

add_custom_target(run
    COMMAND qemu-system-x86_64 -M q35 -m 512M -cdrom ${CMAKE_BINARY_DIR}/kernel.iso -serial stdio
    DEPENDS iso
    COMMENT "Ejecutando QEMU..."
)
