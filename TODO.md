# Evilos - TODO

Roadmap operativa. Este archivo describe el estado actual y el siguiente trabajo útil. Las decisiones especulativas permanecen fuera del camino crítico.

---

# 0. Estado actual

## Kernel / boot

- [x] Limine v11
- [x] Entrada x86-64
- [x] C runtime freestanding básico
- [x] UART
- [x] Framebuffer
- [x] Flanterm
- [x] Memory map
- [x] HHDM
- [x] Kernel physical/virtual address
- [x] ACPI RSDP discovery

## CPU / exceptions

- [x] GDT
- [x] IDT
- [x] TSS
- [ ] Verificar exhaustivamente handlers de excepciones
- [ ] Probar #DE
- [ ] Probar #UD
- [ ] Probar #GP
- [ ] Probar #PF
- [ ] Probar double fault de forma controlada
- [ ] Diagnóstico uniforme de registros y contexto

## Memoria

- [x] Buddy allocator
- [x] `kmalloc`
- [x] `kfree`
- [x] Smoke test de allocation/free
- [ ] Suite de stress del allocator
- [ ] Medir fragmentación
- [ ] Probar split/coalescing/reutilización de bloques
- [ ] Definir contrato de ownership

## Primitivas comunes

- [x] Slice
- [x] StringView
- [ ] Revisar API y ownership
- [ ] Añadir tests exhaustivos
- [ ] Documentar invariantes

## Tests

- [x] Unit tests host-side
- [x] Ejecutar algoritmos/primitivas desde el host
- [ ] Añadir self-tests dentro del kernel
- [ ] Unificar convención de nombres y resultado de tests
- [ ] Separar unit, integration y self-test

---

# 1. Prioridad inmediata: hacer confiable lo que ya existe

## 1.1. Buddy / kmalloc

- [ ] Allocation de tamaños pequeños
- [ ] Allocation de tamaños grandes
- [ ] Múltiples allocations simultáneas
- [ ] Free en distinto orden
- [ ] Reutilización de bloques
- [ ] Split de bloques
- [ ] Coalescing con buddy libre
- [ ] Agotamiento deliberado
- [ ] Alineamiento
- [ ] Estadísticas de uso
- [ ] Tests de fragmentación
- [ ] Tests de doble free / free inválido, según el contrato elegido

## 1.2. Slice / StringView

- [ ] Slice vacío
- [ ] Slice completo
- [ ] Sub-slice
- [ ] Límites
- [ ] Length 0
- [ ] StringView con NUL
- [ ] StringView sin NUL
- [ ] Comparación
- [ ] Búsqueda simple
- [ ] Conversión controlada a C string cuando sea necesario
- [ ] Verificar que no introduzcan ownership implícito

## 1.3. Excepciones

- [ ] Definir frame de excepción común
- [ ] Imprimir vector
- [ ] Imprimir error code cuando exista
- [ ] Imprimir RIP/RSP/RFLAGS
- [ ] Imprimir CR2 para #PF
- [ ] Test deliberado por excepción
- [ ] Mantener panic path sin allocation dinámica

---

# 2. Infraestructura de eventos

La siguiente unidad arquitectónica es un camino común para eventos del kernel.

## 2.1. Event types

- [ ] Definir `EventType`
- [ ] Definir eventos de teclado
- [ ] Definir eventos de timer
- [ ] Definir eventos de hardware
- [ ] Definir eventos de syscall
- [ ] Definir eventos de page fault
- [ ] Usar Tagged Unions donde corresponda
- [ ] Evitar un `God Event` con campos irrelevantes

## 2.2. Ring buffer

- [ ] Diseñar ring buffer fijo/preasignado
- [ ] Definir producer/consumer ownership
- [ ] Definir comportamiento ante overflow
- [ ] Definir si es SPSC, MPSC o restringir el primer diseño
- [ ] Tests host-side
- [ ] Self-tests del kernel
- [ ] Medir coste

## 2.3. ISR / Bottom Half

- [ ] Definir trabajo mínimo de ISR
- [ ] Enqueue de evento desde ISR
- [ ] Ack del hardware
- [ ] Procesamiento fuera de ISR
- [ ] Crear una primera ruta hardware -> evento -> consumer
- [ ] Verificar que Lua no se ejecuta desde ISR

---

# 3. Tracing nativo

El tracing será parte de la observabilidad del kernel, no una dependencia de userland.

- [ ] Definir `TraceEventType`
- [ ] Definir `TraceEvent`
- [ ] Incluir timestamp
- [ ] Incluir thread/process identity cuando exista
- [ ] Incluir syscall ID cuando corresponda
- [ ] Evitar punteros persistentes a memoria de procesos
- [ ] Preasignar ring buffer de tracing
- [ ] Emisión sin allocation
- [ ] Registrar syscalls
- [ ] Registrar page faults
- [ ] Registrar page allocation/free
- [ ] Registrar scheduler events
- [ ] Exponer salida por UART
- [ ] Exponer salida por framebuffer/Lua posteriormente
- [ ] Añadir filtros básicos

---

# 4. Interrupciones externas

- [ ] Inicializar controlador de interrupciones
- [ ] Habilitar interrupciones
- [ ] Timer interrupt
- [ ] Keyboard interrupt
- [ ] Contador por IRQ
- [ ] Ruta IRQ -> event ring -> consumer
- [ ] Test de overflow
- [ ] Test con interrupciones rápidas

---

# 5. Memoria virtual

## 5.1. Page tables

- [ ] Crear API mínima para mapping
- [ ] Mapear página
- [ ] Unmapear página
- [ ] Cambiar permisos
- [ ] Consultar mapping
- [ ] Liberar tablas cuando corresponda
- [ ] Test de mapping básico
- [ ] Test de permission fault

## 5.2. Area

- [ ] Definir representación de `Area`
- [ ] Definir intervalo virtual
- [ ] Definir permisos
- [ ] Definir backing
- [ ] Definir commit policy separadamente
- [ ] No acoplar VMM directamente a VFS
- [ ] Resolver búsqueda de Area
- [ ] Definir ownership/lifetime

## 5.3. Backing

- [ ] Anonymous
- [ ] File-backed
- [ ] Shared
- [ ] MMIO
- [ ] Usar referencias/handles estables
- [ ] Mantener backing separado de commit

## 5.4. Commit

- [ ] Lazy allocation
- [ ] Preallocated regions
- [ ] Guard pages como propiedad de mapping/protection
- [ ] Definir significado exacto de "committed"

## 5.5. Page fault

- [ ] Obtener CR2
- [ ] Interpretar error code
- [ ] Buscar Area
- [ ] Validar permisos
- [ ] Resolver anonymous
- [ ] Resolver shared
- [ ] Resolver file-backed cuando exista VFS
- [ ] Resolver MMIO
- [ ] Mapear página
- [ ] Reanudar ejecución cuando sea válido
- [ ] Panic cuando no exista una resolución válida

---

# 6. Drivers y primitivas comunes

## 6.1. Base

- [ ] Utilizar Slice/StringView donde aporten claridad
- [ ] Evitar copias innecesarias
- [ ] Documentar ownership de buffers
- [ ] Definir protocolos explícitos
- [ ] Modelar estados relevantes como FSM

## 6.2. Keyboard

- [ ] Driver de teclado
- [ ] Scancode parsing
- [ ] Key press/release
- [ ] Modifiers
- [ ] Emitir `KeyboardEvent`
- [ ] Consumidor fuera de ISR

## 6.3. Timer

- [ ] Inicializar timer
- [ ] Tiempo monotónico
- [ ] Eventos periódicos
- [ ] Sleep/wakeup básico
- [ ] Integración con scheduler futuro

---

# 7. Gráficos

## 7.1. Primitive drawing

- [ ] DrawPixel
- [ ] Clear
- [ ] Line
- [ ] Rectangle
- [ ] Bitmap font
- [ ] Tests host-side para primitivas puras

## 7.2. Backbuffer

- [ ] Reservar memoria
- [ ] Dibujar solamente en backbuffer
- [ ] Presentar al framebuffer
- [ ] Medir coste de copia

## 7.3. GUI

- [ ] Mantener biblioteca gráfica semánticamente simple
- [ ] Separar dibujo de layout
- [ ] Separar foco de dibujo
- [ ] Definir regiones/buffers
- [ ] Evitar que la biblioteca gráfica conozca el teclado

---

# 8. Lua

## 8.1. Integración básica

- [ ] Integrar Lua freestanding
- [ ] Conectar allocator actual
- [ ] Ejecutar expresión
- [ ] Ejecutar bloque
- [ ] Reportar error
- [ ] Test de agotamiento de memoria

## 8.2. Frontera C <-> Lua

- [ ] Definir API mínima
- [ ] Exponer print
- [ ] Exponer tiempo
- [ ] Exponer eventos
- [ ] Exponer primitivas gráficas cuando existan
- [ ] Validar argumentos
- [ ] Definir ownership de datos C/Lua
- [ ] Evitar exponer estructuras internas sin necesidad

## 8.3. Minibuffer

- [ ] Región de entrada
- [ ] Edición básica
- [ ] Evaluar Lua
- [ ] Capturar error
- [ ] Mostrar resultado
- [ ] Mantener sistema vivo después de error Lua

---

# 9. Scheduler cooperativo

- [ ] Definir tarea Lua
- [ ] Coroutine creation
- [ ] Resume
- [ ] Yield
- [ ] Termination
- [ ] Error state
- [ ] Run queue
- [ ] Wait state
- [ ] Wakeup por evento
- [ ] Integrar timer
- [ ] Nunca ejecutar Lua arbitrariamente desde ISR
- [ ] Probar tarea que no hace yield
- [ ] Documentar que un loop infinito bloquea el modelo cooperativo

---

# 10. Módulos Lua

## Primero: memoria/registro

- [ ] Módulos embebidos
- [ ] Registro de módulos disponibles
- [ ] `require`
- [ ] Cache
- [ ] Missing module
- [ ] Load error
- [ ] Circular dependency

## Después: filesystem

- [ ] Resolver módulo desde FS
- [ ] Mantener la misma interfaz conceptual
- [ ] No acoplar el loader a un único almacenamiento

---

# 11. Filesystem

Implementar solamente cuando el sistema necesite dejar de embebar módulos o exista otra necesidad concreta.

- [ ] Block device interface
- [ ] Read block
- [ ] Write block
- [ ] Estructura mínima de filesystem
- [ ] Create file
- [ ] Read file
- [ ] Write file
- [ ] List
- [ ] Delete
- [ ] Error/corruption handling
- [ ] Integrar loader de módulos

---

# 12. Network

No empezar hasta que memoria, eventos y planificación sean utilizables.

- [ ] Packet buffer representation
- [ ] Slice sobre buffers cuando corresponda
- [ ] NIC driver
- [ ] RX event
- [ ] TX path
- [ ] Ethernet
- [ ] ARP
- [ ] IPv4
- [ ] ICMP
- [ ] UDP
- [ ] TCP solamente si una aplicación realmente lo necesita
- [ ] Primera aplicación simple

---

# 13. Compartición y zero-copy

Antes de optimizar, establecer la baseline con copia.

- [ ] Definir ownership de buffers
- [ ] Compartir una página
- [ ] Reference tracking
- [ ] Liberación cuando no quedan consumidores
- [ ] Medir copy vs share
- [ ] Aplicar a buffers de red
- [ ] Evaluar buffers gráficos
- [ ] Documentar cuándo la complejidad compensa

---

# 14. Páginas grandes y metadata agrupada

Esto continúa siendo un experimento, no un compromiso arquitectónico.

- [ ] Implementar baseline con metadata simple
- [ ] Medir metadata por página
- [ ] Probar metadata agrupada
- [ ] Medir lookup
- [ ] Medir alloc/free
- [ ] Medir split
- [ ] Medir sharing
- [ ] Medir fragmentación
- [ ] Comparar resultados
- [ ] Registrar decisión

La decisión final puede ser mantener la implementación simple.

---

# 15. Allocator de bloques futuro

El Buddy ya existe y `kmalloc` funciona sobre él. No introducir otro allocator solamente por arquitectura estética.

- [ ] Obtener workload real de allocations
- [ ] Medir tamaños frecuentes
- [ ] Medir fragmentación
- [ ] Medir coste de allocation/free
- [ ] Evaluar segregated free lists
- [ ] Evaluar TLSF
- [ ] Comparar contra allocator actual
- [ ] Migrar solamente si existe una mejora demostrable

---

# 16. SMP / paralelismo

No es camino crítico.

- [ ] Detectar CPUs
- [ ] Arrancar CPU secundaria
- [ ] Stack por CPU
- [ ] Estado por CPU
- [ ] Identificar estructuras globales
- [ ] Identificar estructuras per-CPU
- [ ] Probar allocations concurrentes
- [ ] Proteger estructuras compartidas
- [ ] Medir contención
- [ ] Scheduler multicore cuando sea necesario

Después:

- [ ] pools per-CPU
- [ ] caches locales
- [ ] estructuras lock-free solamente donde estén justificadas por mediciones

---

# 17. Tests internos

Esta fase debe crecer junto con los subsistemas, no quedar al final.

## Self-test en boot

- [ ] Assertion básica
- [ ] Resultado PASS/FAIL uniforme
- [ ] Test de Slice
- [ ] Test de StringView
- [ ] Test de Buddy
- [ ] Test de kmalloc/kfree
- [ ] Test de ring buffer
- [ ] Test de FSMs críticas

## Integration tests

- [ ] memmap -> Buddy
- [ ] Buddy -> kmalloc
- [ ] IRQ -> event queue
- [ ] page fault -> Area
- [ ] C API -> Lua
- [ ] event -> scheduler
- [ ] module -> error recovery

## Regla

Los tests host-side deben ser rápidos. Los self-tests deben verificar propiedades que solamente existen dentro del kernel. Los integration tests deben probar las fronteras entre subsistemas.

---

# 18. Observabilidad

- [ ] kprintf uniforme
- [ ] panic uniforme
- [ ] exception report
- [ ] memory stats
- [ ] allocator stats
- [ ] event queue stats
- [ ] tracing
- [ ] filtros de tracing
- [ ] comandos de inspección desde Lua

Objetivo eventual:

```text
kernel state
    |
    +--> UART
    +--> framebuffer
    +--> trace buffer
    +--> Lua inspection
```

---

# 19. Criterio para cerrar una tarea

Una tarea no está "hecha" porque el código compile.

Debe terminar en una de estas condiciones:

```text
FUNCIONA
```

```text
FALLA DE FORMA CONOCIDA
```

```text
HIPÓTESIS DESCARTADA
```

La tarea debe dejar evidencia suficiente para reproducir el resultado.

---

# 20. Hito funcional inmediato

El siguiente sistema funcional de interés es:

```text
Limine
  |
  v
kernel C
  |
  +--> GDT / IDT / TSS
  +--> memory map
  +--> Buddy / kmalloc
  +--> framebuffer
  +--> IRQ
  |
  v
event ring
  |
  +--> keyboard
  +--> timer
  |
  v
Lua
  |
  +--> UI
  +--> shell / minibuffer
  +--> cooperative tasks
```

Sin filesystem ni red.

Después de ese punto, PMM/VMM/Area, módulos externos, filesystem, red, sharing, zero-copy y SMP pueden evolucionar sobre una base ya observable.

---

# 21. Regla de alcance

Una nueva característica entra al TODO solamente si:

```text
una capa existente la necesita
        OR
existe un experimento concreto que la justifica
```

No abrir una fase para una tecnología solamente porque podría ser útil algún día.

Ejemplo:

```text
"algún día podríamos necesitar NUMA"
```

no implica implementar NUMA.

La arquitectura debe dejar espacio para el futuro sin pagar su complejidad por adelantado.

---

# 22. Orden recomendado de trabajo

Cuando haya varias opciones disponibles, priorizar:

1. corregir o probar la infraestructura ya existente;
2. construir primitivas pequeñas y reutilizables;
3. crear una ruta observable de extremo a extremo;
4. separar hardware de procesamiento mediante eventos;
5. implementar VMM y Area cuando sean realmente necesarias;
6. integrar Lua sobre contratos pequeños;
7. optimizar solamente después de medir;
8. añadir filesystem, red y SMP cuando exista una necesidad concreta.

El orden puede cambiar. Las dependencias reales mandan.
