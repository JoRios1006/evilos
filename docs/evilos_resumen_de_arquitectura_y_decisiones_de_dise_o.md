# Documento de Arquitectura y Decisiones de Diseño - Proyecto Evilos

**Fecha de registro:** Octubre 2026
**Fase actual:** Fase 3 (Gestión de Memoria Inicial) y Preparación para Fase 4.
**Arquitectura de destino:** x86_64 (Freestanding, Bare-metal)
**Bootloader:** Limine (Protocolo v3)

---

## 1. Contexto General del Proyecto

Evilos es un sistema operativo experimental de 64 bits desarrollado desde cero. El entorno de desarrollo utiliza **Clang** como compilador principal y **LLD** como enlazador, asegurando un entorno estrictamente *freestanding* (sin dependencias de la biblioteca estándar del host). La ejecución y depuración se realizan mediante **QEMU**, con salida dual a través de un emulador de terminal gráfico integrado (**Flanterm** vía Framebuffer) y salida de texto por puerto serie (**UART**).

Hasta el inicio de este registro, el kernel había completado con éxito las Fases 1 y 2, logrando:
*   Punto de entrada estructurado en C.
*   Resolución de peticiones de Limine (Mapa de memoria, HHDM, Base del Kernel, RSDP).
*   Configuración canónica de la GDT (Global Descriptor Table) con TSS.
*   Configuración de la IDT (Interrupt Descriptor Table) y manejo básico de ISRs.

---

## 2. Gestión de Memoria Inicial (La Fase 3)

### 2.1. El rechazo temporal de `mlibc`
La intención inicial era portar `mlibc` (una biblioteca C estándar completa) al kernel para obtener acceso rápido a funciones como `malloc`. 
**Decisión:** Se pospuso la integración de `mlibc`.
**Justificación:** `mlibc` requiere infraestructura avanzada que Evilos aún no posee (manejo de memoria virtual/paginación completa, carga de binarios ELF, y un mecanismo de *syscalls* Ring 3 a Ring 0).

### 2.2. La adopción de `buddy_alloc`
Para cumplir con el objetivo de la Fase 3 ("Conseguir memoria dinámica sin implementar todavía PMM/VMM"), se decidió integrar `buddy_alloc`, un asignador de memoria binario (*buddy allocator*).
**Decisión:** Utilizar `buddy_alloc` en modo *embebido* (`buddy_embed`).
**Justificación:** Al no tener un asignador previo para alojar la metadata del árbol del *buddy allocator*, la función `buddy_embed` permite incrustar dicha metadata dentro del propio bloque de memoria física que va a administrar.

**Mecanismo de secuestro de RAM:**
1. Se iteró sobre el mapa de memoria proporcionado por Limine.
2. Se identificó el bloque físico de tipo `USABLE` más grande (ej. ~509 MB).
3. **Traducción HHDM:** Se sumó el offset del *Higher Half Direct Map* (`hhdm_req.response->offset`) a la dirección física base para obtener un puntero virtual válido que evite *Page Faults*.
4. Se envolvió la implementación en una interfaz propia (`kmalloc.h` / `kmalloc.c`) para aislar la dependencia y permitir un futuro reemplazo (ej. por TLSF) sin afectar al resto del kernel.

---

## 3. Desafíos del Sistema de Construcción (CMake)

Durante la integración de `buddy_alloc`, surgieron conflictos con **CMake**:
*   **Problema:** Usar `FetchContent_MakeAvailable` ejecutaba el `CMakeLists.txt` de la librería externa, el cual asumía un entorno host (Linux) y corrompía la caché del compilador, intentando usar GCC y buscando cabeceras estándar como `<stdio.h>`.
*   **Solución Arquitectónica:** 
    1. Se descartó el submódulo de CMake para esta librería.
    2. Se adoptó el patrón de integración manual (estilo *stb header-only*).
    3. Se forzó estrictamente a CMake a usar `clang` y `ld.lld` **antes** de la declaración `project()`.
    4. Se definió `BUDDY_PRINTF` apuntando a `kprintf` y se declaró externamente antes de invocar `#define BUDDY_ALLOC_IMPLEMENTATION` para evitar la inclusión indeseada de librerías del host.
    5. Se corrigió el enlace de variables globales (`memmap_req`, `hhdm_req`) eliminando el modificador `static` en `kernel.c` para darles enlace externo (*external linkage*).

---

## 4. Infraestructura de Pruebas (Test Runner en Lua)

Para garantizar la estabilidad ante refactorizaciones, se debatió sobre la implementación de pruebas unitarias para pánicos del kernel e interrupciones.

### 4.1. Filosofía de Testing
*   **Pruebas internas (Futuro):** Para la lógica de negocio (estructuras de datos, formateo, coalescencia de memoria), el kernel debe ejecutar aserciones internas antes de ceder el control.
*   **Pruebas externas (Host-Guest):** Para verificar los *Kernel Panics* (ej. Page Faults, Double Faults), un observador externo es obligatorio, ya que el procesador se detiene (`hlt`) y el código interno no puede evaluar su propia muerte de forma segura.

### 4.2. Elección de Lua y luaposix
**Decisión:** Desarrollar el *Test Runner* del host en Lua en lugar de Python.
**Justificación:** Lua será el lenguaje de *scripting* integrado por defecto dentro del kernel Evilos. Usarlo también en el host mantiene la consistencia de herramientas y principios del proyecto.
**Implementación:** Se utilizó `luaposix` para sortear las limitaciones de la biblioteca estándar de Lua. El script realiza un `fork`, redirige la salida del emulador mediante tuberías (`pipe`), y ejecuta QEMU de forma oculta (`execp`). Un temporizador por hardware (`alarm`) protege contra bucles infinitos. El script parsea la salida del UART y finaliza con éxito si detecta el log esperado.

---

## 5. Prevención de Data Faults y Concurrencia

Se analizó la problemática de los *Data Faults* documentados en la industria: errores sutiles donde las operaciones matemáticas (ej. `counter++`) son interrumpidas en medio de su ciclo de lectura-modificación-escritura (*read-modify-write*).

**Directivas establecidas para el futuro:**
*   El modificador `volatile` no garantiza atomicidad, solo previene la optimización del caché por parte del compilador.
*   Se requerirá el uso de `cli` (Clear Interrupts) y `sti` (Set Interrupts) para proteger secciones críticas en sistemas mononúcleo.
*   Se requerirán *Spinlocks* (ej. instrucción atómica `lock cmpxchg`) al dar el salto a SMP (Symmetric Multiprocessing).
*   Uso de tipos atómicos de C (`<stdatomic.h>`) para contadores compartidos entre ISRs y el bucle principal.

---

## 6. Modernización de la Biblioteca Estándar (`libk`)

Para prevenir *buffer overflows*, eliminar la dependencia del terminador nulo (`\0`) y preparar el kernel para el desarrollo de drivers de red, se rediseñó el manejo de cadenas.

### 6.1. Slices y StringViews
**Decisión:** Implementar estructuras inmutables `StringView` (para texto) y `Slice` (para datos binarios).
**Justificación:** Permiten la manipulación de memoria con cero copias (*zero-copy*). Especialmente útil para parsear paquetes de red donde una estructura empaquetada (`__attribute__((packed))`) se superpone sobre un buffer crudo, y el *payload* se pasa a las capas superiores como un Slice seguro.

### 6.2. Especificador de formato personalizado (`%v`)
Para integrar `StringView` nativamente, se modificó el motor `vsnprintf` de Evilos añadiendo el especificador `%v`. Esto permite a `kprintf` imprimir secciones de memoria limitadas estrictamente por su propiedad `length`, evitando el colapso del sistema por cadenas sin terminador nulo.

---

## 7. Endurecimiento del Código (Code Review)

Tras una auditoría exhaustiva de solidez, se implementaron mejoras críticas en la Fase 3:
1. **Validación de errores:** `kmalloc_init` ahora aborta el arranque (`panic`) si la arena de memoria es demasiado pequeña o si `buddy_embed` devuelve `NULL`.
2. **Pruebas de estrés integradas:** Se creó la función `test_kmalloc_stress` para validar no solo el "happy path" de asignar y liberar un bloque, sino para probar la correcta alineación de los punteros (8 bytes), la coalescencia de memoria libre, y la carga iterativa masiva de datos.

---

## Próximos Pasos (Fase 4 y posteriores)
Con una infraestructura de pruebas automatizada, una gestión de cadenas segura mediante Slices, y un asignador dinámico de memoria funcional y estresado, el kernel está preparado para:
1. Parsear las tablas ACPI (RSDP interceptado con éxito).
2. Descubrir la topología del procesador vía MADT.
3. Configurar el Temporizador del Sistema (APIC/PIT) y adentrarse en la arquitectura del planificador (Scheduler).