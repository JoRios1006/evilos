#include <uacpi/kernel_api.h>
#include "../memory/kmalloc.h"
#include "../limine.h"

// Funciones externas de Evilos
extern void kprintf(const char *format, ...);
extern volatile struct limine_hhdm_request hhdm_req;
extern volatile struct limine_rsdp_request rsdp_req;

// -----------------------------------------------------------------------------
// MEMORIA DINÁMICA (Conectado a Buddy Allocator)
// -----------------------------------------------------------------------------
void *uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc(size);
}

void uacpi_kernel_free(void *mem) {
    kfree(mem);
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if (rsdp_req.response == NULL || hhdm_req.response == NULL) {
        return UACPI_STATUS_NOT_FOUND;
    }
    
    // Convertimos la dirección virtual devuelta por Limine a la física real
    uintptr_t virtual_rsdp = (uintptr_t)rsdp_req.response->address;
    *out_rsdp_address = (uacpi_phys_addr)(virtual_rsdp - hhdm_req.response->offset);
    return UACPI_STATUS_OK;
}
// -----------------------------------------------------------------------------
// MEMORIA VIRTUAL (Conectado al Higher Half Direct Map de Limine)
// -----------------------------------------------------------------------------
void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    (void)len;
    if (hhdm_req.response == NULL) return NULL;
    // En Evilos, cualquier dirección física se accede sumando el offset HHDM
    return (void *)(addr + hhdm_req.response->offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    (void)addr;
    (void)len;
    // El mapa HHDM es estático y cubre toda la memoria. No hay nada que liberar.
}

// -----------------------------------------------------------------------------
// LOGS Y DEPURACIÓN
// -----------------------------------------------------------------------------
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *str) {
    (void)level;
    // Redirigir la salida interna de uACPI a nuestro emulador de terminal VT100
    kprintf("%s", str);
}

// -----------------------------------------------------------------------------
// TIEMPO (Stubs para cumplir con la API básica)
// -----------------------------------------------------------------------------
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    // TODO: Tarea 6.3 - Implementar Timer monotónico (PIT o APIC)
    return 0; 
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    (void)usec;
    // TODO: Tarea 6.3 - Implementar retardo por hardware
}
