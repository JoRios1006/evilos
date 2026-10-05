cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_ASM_COMPILER "clang")
set(CMAKE_LINKER "ld.lld")

project(osdev_kernel C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    limine
    GIT_REPOSITORY https://github.com/limine-bootloader/limine.git
    GIT_TAG v11.x-binary
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(limine)

FetchContent_Declare(
    flanterm
    GIT_REPOSITORY https://github.com/Mintsuki/Flanterm.git
    GIT_TAG trunk
)
FetchContent_MakeAvailable(flanterm)

FetchContent_Declare(
    uacpi
    GIT_REPOSITORY https://github.com/uACPI/uACPI.git
    GIT_TAG master
)
FetchContent_MakeAvailable(uacpi)

include(${uacpi_SOURCE_DIR}/uacpi.cmake)

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

add_executable(kernel.elf
    src/kernel.c
    src/libk.c
    src/drivers/uart.c
    src/arch/x86_64/gdt.c
    src/arch/x86_64/gdt_flush.S
    src/arch/x86_64/idt.c
    src/arch/x86_64/isr_stubs.S
    src/arch/x86_64/isr.c
    src/memory/buddy_alloc.c
    src/memory/kmalloc.c
    src/uacpi_hooks/kernel_api.c
    ${flanterm_SOURCE_DIR}/src/flanterm.c
    ${flanterm_SOURCE_DIR}/src/flanterm_backends/fb.c
    ${UACPI_SOURCES}
)

target_include_directories(kernel.elf PRIVATE
    ${flanterm_SOURCE_DIR}/src
    src
    src/memory
    src/uacpi_hooks
    ${UACPI_INCLUDES}
)

target_compile_definitions(kernel.elf PRIVATE
    UACPI_OVERRIDE_LIBC=1
    UACPI_BAREBONES_MODE=1
)

target_compile_options(kernel.elf PRIVATE ${KERNEL_CFLAGS})
target_link_options(kernel.elf PRIVATE ${KERNEL_LDFLAGS} "LINKER:--error-limit=0")

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
    DEPENDS kernel.elf
    COMMENT "Generando ISO con Limine..."
)

add_custom_target(run
    COMMAND qemu-system-x86_64 -M q35 -m 512M -cdrom ${CMAKE_BINARY_DIR}/kernel.iso -serial stdio
    DEPENDS iso
    COMMENT "Ejecutando QEMU..."
)
