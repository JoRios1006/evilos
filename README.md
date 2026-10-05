# evilos
Emacs Vi Layered Operating System

# RFC: Arquitectura del sistema experimental

**Estado:** Working Draft

**Plataforma actual:** x86-64

**Boot:** Limine v11

**Implementación:** Parcial y evolutiva

---

## 1. Resumen

Evilos es un sistema experimental para x86-64 con un núcleo pequeño escrito en C y Lua como lenguaje para buena parte de la lógica de alto nivel.

El objetivo no es construir un sistema operativo generalista ni reproducir la arquitectura de otro kernel. El objetivo es disponer de un sistema pequeño, observable y modificable, en el que las fronteras entre mecanismos, datos y lógica sean explícitas.

La arquitectura sigue una regla simple: primero debe existir una implementación que funcione y pueda medirse; después se decide si merece la pena generalizarla u optimizarla.

La organización del sistema se apoya en:

```text
hardware
   |
   v
C / mecanismos del kernel
   |
   +--> memoria
   +--> interrupciones
   +--> drivers
   +--> colas / eventos
   +--> primitives
   |
   v
Lua / lógica de alto nivel
   |
   v
UI, shell, módulos y coordinación
```

La dirección general no es una jerarquía rígida de capas. Los datos y eventos deben poder atravesar las capas intermedias sin que estas inventen semántica que pertenece al consumidor.

---

## 2. Estado actual

El núcleo ya dispone de una base funcional sobre la que continuar el desarrollo.

Actualmente implementado o integrado:

```text
[x] Limine
[x] entrada x86-64
[x] runtime C freestanding básico
[x] UART
[x] framebuffer + Flanterm
[x] memory map
[x] HHDM
[x] kernel physical/virtual address
[x] ACPI RSDP discovery
[x] GDT
[x] IDT
[x] TSS
[x] Buddy allocator
[x] kmalloc / kfree sobre el allocator de memoria
[x] prueba básica de allocation/free
[x] unit tests ejecutables fuera del kernel
[x] base Slice
[x] base StringView
```

La implementación actual ya permite arrancar el kernel, obtener la descripción inicial de memoria y utilizar asignación dinámica del kernel.

La memoria dinámica actual no debe confundirse con la arquitectura final de memoria virtual. El siguiente salto importante es separar claramente memoria física, memoria virtual, regiones y asignación de bloques.

---

## 3. Objetivos arquitectónicos

El sistema deberá:

* arrancar mediante Limine;
* inicializar explícitamente el estado de CPU necesario;
* administrar memoria física;
* administrar memoria virtual;
* proporcionar asignación dinámica de bloques;
* procesar hardware mediante interrupciones mínimas y trabajo posterior fuera del contexto de interrupción;
* utilizar estructuras de datos explícitas y pequeñas;
* representar estados importantes mediante máquinas de estados finitos;
* comunicar subsistemas desacoplados mediante eventos, colas o protocolos cuando corresponda;
* ejecutar Lua dentro del kernel;
* exponer a Lua una frontera C pequeña y explícita;
* construir una interfaz basada en buffers y regiones;
* permitir inspeccionar y modificar parte del estado del sistema durante la ejecución;
* mantener una infraestructura de pruebas tanto fuera como dentro del kernel;
* permitir reemplazar implementaciones concretas sin obligar a rediseñar las capas superiores.

No se considera requisito que todos los subsistemas utilicen todas estas técnicas. Son herramientas de diseño, no una plantilla obligatoria.

---

## 4. Principios de diseño

### 4.1 End-to-End: endpoints inteligentes, transporte simple

Las capas intermedias deben transportar, ordenar, almacenar o proteger datos sin intentar anticipar la semántica futura del consumidor.

La lógica específica debe permanecer en el extremo que realmente conoce el significado de los datos.

Por ejemplo:

```text
hardware
   |
   v
interrupt handler
   |
   v
[event queue]
   |
   v
consumer / FSM
```

La cola no necesita conocer el significado completo del evento. Su responsabilidad es transportar la representación acordada.

Esto evita que una infraestructura central acumule reglas de todos los consumidores.

---

### 4.2 Datos y primitivas reales antes que patrones de objetos

El kernel debe preferir estructuras de datos, funciones y protocolos explícitos antes que jerarquías de objetos creadas para representar relaciones que pueden expresarse directamente.

Las herramientas principales son:

```text
structs
unions
Tagged Unions
arrays
ring buffers
queues
trees
FSMs
protocolos
handshakes
```

Los patrones de diseño no se consideran una restricción arquitectónica. Si una solución puede expresarse directamente mediante datos + estado + funciones, no se necesita introducir una abstracción equivalente a un patrón orientado a objetos.

---

### 4.3 Estados explícitos

Cuando un componente tiene estados relevantes, estos deben poder identificarse y sus transiciones deben ser definibles.

Una FSM es preferible cuando ayuda a hacer imposibles estados inválidos o a comprobar de forma directa el protocolo de un componente.

Ejemplo conceptual:

```text
RESET
  |
  v
INITIALIZING
  |
  +---- error ----> FAILED
  |
  v
READY
  |
  v
RUNNING
```

No todo código necesita una FSM explícita. Una FSM se introduce cuando el estado y las transiciones son parte del comportamiento que se quiere controlar.

---

### 4.4 Ring buffers en caminos sensibles al tiempo

Los caminos de hardware o streaming deben evitar allocations dinámicas cuando una estructura preasignada puede resolver el problema.

Un ring buffer es una opción preferida para:

* eventos de interrupción;
* tracing;
* comunicación producer/consumer;
* buffers de entrada;
* colas de trabajo pequeñas y predecibles.

La preasignación permite que el camino crítico no dependa del estado del allocator.

La localidad de memoria es una propiedad que debe medirse, no una garantía automática de usar un ring buffer.

---

### 4.5 Protocolos y handshakes

Los componentes independientes deben comunicarse mediante contratos explícitos.

Un protocolo debe describir al menos:

```text
qué entra
qué sale
qué estados existen
qué transiciones son válidas
qué ocurre ante error
quién posee los datos
cuándo termina una operación
```

Los protocolos de hardware, drivers, IPC y subsistemas internos pueden representarse mediante FSMs cuando su complejidad lo justifique.

---

### 4.6 Datagramas y Tagged Unions

Cuando un evento debe atravesar varias capas, se prefiere una representación autocontenida y explícita.

Ejemplo conceptual:

```c
typedef enum {
    EVENT_KEYBOARD,
    EVENT_TIMER,
    EVENT_SYSCALL,
    EVENT_PAGE_FAULT,
} EventType;

typedef struct {
    EventType type;
    union {
        KeyboardEvent keyboard;
        TimerEvent timer;
        SyscallEvent syscall;
        PageFaultEvent page_fault;
    } data;
} KernelEvent;
```

La unión etiquetada evita convertir un mensaje genérico en una estructura que contenga información irrelevante para casi todos los casos.

Un mensaje autocontenido facilita el registro, las pruebas, la depuración y el paso de un evento entre componentes. No implica por sí solo transparencia de ubicación ni serialización automática.

---

### 4.7 Top Half / Bottom Half

Las rutinas de interrupción deben hacer la cantidad mínima de trabajo necesaria para registrar el evento, reconocer el hardware y devolver el control.

El trabajo complejo debe continuar fuera del contexto de interrupción.

La forma conceptual preferida es:

```text
hardware
   |
   v
Top Half / ISR
   |
   +--> read hardware state
   +--> build event
   +--> enqueue
   +--> acknowledge
   |
   v
return

Bottom Half / worker
   |
   v
FSM / subsystem / Lua
```

No se ejecutará Lua arbitrariamente desde una ISR.

---

### 4.8 Contratos antes que implementación

Los componentes importantes se describirán mediante documentos tipo RFC, estados, invariantes y pruebas antes de crecer indefinidamente en código.

La documentación debe responder al menos:

```text
qué hace
por qué existe
qué recibe
qué produce
qué puede fallar
qué invariantes mantiene
cómo se prueba
```

UML no se considera la representación principal del sistema. Los diagramas de estado, flujos de datos y contratos textuales son preferibles cuando expresan directamente el comportamiento relevante.

---

### 4.9 Verificación en capas

Las pruebas deben ejecutarse en el nivel más barato que todavía pueda verificar la propiedad.

```text
host-side unit test
        |
        v
kernel self-test
        |
        v
integration test
        |
        v
hardware / QEMU
```

Los tests host-side sirven para desarrollar estructuras y algoritmos rápidamente.

Los tests internos verifican propiedades que dependen del ABI, memoria, interrupciones o entorno real del kernel.

Ninguna de las dos categorías sustituye completamente a la otra.

---

### 4.10 Desarrollo V-Kanban

La gobernanza macro seguirá una variante simple del Modelo en V y la ejecución cotidiana seguirá un flujo Kanban.

```text
RFC / contrato
      |
      v
invariantes + casos de prueba
      |
      v
implementación
      |
      v
pruebas
      |
      v
integración
```

Cada tarea de trabajo debe ser suficientemente pequeña como para tener un resultado observable. El backlog debe evitar tareas vagas como "trabajar en memoria" y reemplazarlas por resultados verificables.

Cada commit importante debería dejar el sistema en un estado arrancable o, cuando se trate de componentes host-side, en un estado con tests reproducibles.

---

# 5. Arquitectura de memoria

## 5.1. Estado actual

El allocator actual ya utiliza un Buddy allocator y `kmalloc`/`kfree` operan sobre esa infraestructura.

La función de esta capa es administrar bloques utilizables por el kernel. No debe confundirse con la futura gestión de regiones virtuales.

La separación objetivo es:

```text
physical memory
      |
      v
    Buddy
      |
      v
virtual mappings
      |
      v
     Area
      |
      v
block allocator / kmalloc
```

El orden exacto entre VMM, Area y mecanismos de obtención de nuevas regiones se mantendrá ajustable durante la implementación.

---

## 5.2. Buddy allocator

Buddy administra bloques de memoria cuyo tamaño sigue una potencia de dos.

Su utilidad principal es obtener y devolver bloques físicos contiguos de manera sencilla y predecible.

La unidad mínima prevista es una página de 4 KiB.

Conceptualmente:

```text
order 0 = 4 KiB
order 1 = 8 KiB
order 2 = 16 KiB
...
```

La implementación actual se considera infraestructura real, no una fase temporal que deba conservarse como ficción mientras se implementa otro allocator.

---

## 5.3. Memoria virtual y Area

La memoria virtual debe separar:

```text
virtual address
physical backing
permissions
lifetime / ownership
```

La abstracción `Area` se considera un candidato principal para representar regiones virtuales.

Conceptualmente una `Area` puede describir:

```text
range
permissions
backing
commit policy
optional guards
```

El backing y la política de commit deben permanecer separados.

Un backing describe de dónde provienen las páginas:

```text
anonymous
file-backed
shared
MMIO
```

Una política de commit describe cuándo o cómo se obtiene respaldo físico.

No deben multiplicarse enums para representar todas las combinaciones posibles.

---

## 5.4. Page fault y Areas

La integración prevista de memoria virtual utiliza el page fault como una ruta de resolución, no necesariamente como un error fatal.

Flujo conceptual:

```text
page fault
    |
    v
find Area
    |
    +--> invalid permission? -> fault
    |
    +--> anonymous -> allocate
    |
    +--> file-backed -> load
    |
    +--> shared -> obtain backing
    |
    +--> MMIO -> map
    |
    v
map page
```

La implementación concreta debe aparecer después de que PMM, page tables y representación de regiones estén suficientemente probados.

---

## 5.5. Páginas grandes y metadata agrupada

Las páginas grandes y la metadata agrupada siguen siendo hipótesis a medir.

No se asumirán mejoras solamente porque reduzcan el número de estructuras.

Los experimentos deben medir al menos:

```text
metadata bytes
allocation cost
free cost
fragmentation
sharing cost
split cost
lookup cost
```

La decisión final debe venir de mediciones.

---

## 5.6. Asignación de bloques

El allocator de bloques y el Buddy resuelven problemas diferentes.

```text
Buddy
    -> bloques de memoria de granularidad física

kmalloc
    -> objetos y bloques de tamaños arbitrarios del kernel
```

Un allocator segregado o TLSF puede evaluarse posteriormente para allocations pequeñas y frecuentes, especialmente cuando Lua requiera mayor volumen de objetos pequeños.

No se considera decidido sustituir inmediatamente el allocator actual por TLSF.

---

# 6. Tipos fundamentales

Se está construyendo una base pequeña de tipos reutilizables antes de implementar drivers y protocolos más complejos.

## Slice

Un `Slice` representa una vista sobre una secuencia de elementos sin asumir ownership.

Debe dejar explícitos al menos:

```text
pointer
length
```

La representación exacta depende del tipo de dato y del ABI utilizado.

## StringView

`StringView` representa una vista no propietaria sobre texto y evita exigir terminación NUL en operaciones que solamente necesitan longitud + datos.

Estas primitivas se utilizarán como base para parsers, drivers, protocolos y APIs internas donde copiar buffers no sea necesario.

La regla es mantenerlas pequeñas y sin asumir ownership implícito.

---

# 7. Eventos, colas y tracing

Los eventos del sistema deben poder transportarse mediante estructuras explícitas y colas preasignadas.

Un posible flujo general es:

```text
producer
   |
   v
KernelEvent
   |
   v
ring buffer
   |
   v
consumer
```

El mismo modelo puede utilizarse para teclado, timer, drivers y otras fuentes de eventos.

## 7.1. Tracing nativo

Evilos puede disponer de tracing dentro del kernel sin requerir un programa equivalente a `strace` ejecutándose en userland.

El camino previsto es:

```text
syscall / exception / allocator / scheduler
                |
                v
          TraceEvent
                |
                v
        preallocated ring buffer
                |
                v
      UART / framebuffer / Lua
```

El tracer no debe depender de allocations dinámicas en el camino que está intentando observar.

Los eventos pueden representar, entre otros:

```text
syscall
page fault
page allocation
scheduler event
interrupt
```

Las estructuras de eventos deben evitar guardar punteros de memoria arbitrarios como si fueran referencias persistentes. Cuando la identidad de un recurso sea necesaria, debe preferirse un identificador estable o una copia limitada de datos.

---

# 8. Drivers y comunicación con hardware

Los drivers deben mantener una frontera clara entre:

```text
hardware-specific mechanism
        |
        v
event / protocol
        |
        v
consumer
```

La ISR no debería implementar la lógica completa del dispositivo.

Para cada driver se debe poder documentar:

```text
hardware state
initialization states
interrupt states
commands
responses
error states
```

Cuando el protocolo sea suficientemente complejo, se representará como FSM.

`Slice` y `StringView` se consideran primitivas reutilizables para los drivers que procesen buffers o estructuras de datos delimitadas.

---

# 9. Framebuffer y representación gráfica

Limine proporciona un framebuffer lineal. El código gráfico debe escribir sobre un buffer controlado por el sistema y presentar el resultado cuando corresponda.

Flujo conceptual:

```text
Lua / UI
   |
   v
graphics primitives
   |
   v
backbuffer
   |
   v
framebuffer
```

La biblioteca gráfica no debe conocer la política de ventanas, foco, buffers de edición o teclado.

Su responsabilidad debe limitarse a primitivas gráficas, texto bitmap y operaciones necesarias para representar el resultado.

---

# 10. Lua

Lua se ejecutará dentro del espacio de direcciones del kernel.

C iniciará el runtime y proporcionará las operaciones que requieran acceso a mecanismos del kernel.

La frontera debe ser pequeña:

```text
Lua
 |
 v
explicit C API
 |
 v
kernel mechanism
 |
 v
hardware / memory / events
```

Lua no debe conocer estructuras internas que no necesite.

Un error normal del runtime Lua puede aislarse como error del módulo o de la operación en curso. Esto no implica aislamiento frente a corrupción de memoria, bugs en C, punteros inválidos o fallos del propio kernel.

---

# 11. Interfaz y modelo de ejecución

La interfaz seguirá un modelo de buffers y regiones rectangulares antes que un sistema de ventanas flotantes complejo.

La UI y la shell serán candidatos naturales para Lua.

La planificación inicial será cooperativa para las tareas Lua.

```text
Task A -> run -> yield
Task B -> run -> yield
Task C -> run -> yield
```

Un loop infinito que nunca hace `yield` puede bloquear el sistema cooperativo. Esa propiedad debe documentarse y probarse antes de considerar introducir preemption.

El timer puede generar eventos y medir tiempo, pero no debe ejecutar Lua arbitrariamente desde una ISR.

---

# 12. Módulos

La lógica de alto nivel se organizará como módulos Lua.

La primera implementación puede cargar módulos embebidos. Posteriormente podrán cargarse desde filesystem sin cambiar necesariamente la interfaz conceptual de `require`.

El loader debe distinguir entre:

```text
module missing
module load error
module runtime error
module success
```

La modularidad es también una herramienta de diagnóstico: un fallo de Lua debe producir un estado observable que permita determinar qué módulo falló y en qué etapa.

---

# 13. Modelo de fallos

Evilos distingue al menos:

```text
Lua error
    -> error recuperable del módulo, cuando sea posible

C error
    -> puede comprometer el kernel

memory / CPU fault
    -> mecanismo propio de excepción
```

El objetivo no es prometer aislamiento que el kernel todavía no posee.

La recuperación debe implementarse solamente donde el sistema pueda garantizar que el estado restante sigue siendo válido.

---

# 14. Desarrollo y evolución

La implementación debe evolucionar de forma incremental.

Una nueva característica entra cuando resuelve una necesidad concreta de una capa existente o cuando existe un experimento explícito que justifica incorporarla.

Ejemplo:

```text
Lua necesita memoria
    |
    v
allocator
    |
    v
regiones virtuales
    |
    v
páginas
    |
    v
PMM
```

La arquitectura puede dejar espacio para NUMA, SMP, COW, zero-copy, filesystem avanzado u otros mecanismos sin implementarlos de antemano.

Las optimizaciones deben seguir:

```text
hipótesis
    |
    v
implementación mínima
    |
    v
medición
    |
    v
comparación
    |
    v
decisión
```

---

# 15. Cuestiones abiertas

Siguen abiertas, entre otras:

1. La forma final de las `Area` y sus índices.
2. Cómo se obtiene memoria física adicional desde el VMM.
3. La política exacta de commit lazy/preallocated/guard.
4. El comportamiento detallado de page faults recuperables.
5. Si un allocator segregado o TLSF aporta ventajas suficientes sobre el allocator actual.
6. La representación final de memoria compartida.
7. El protocolo de eventos entre drivers y consumidores.
8. La forma exacta del ring buffer genérico y sus reglas de ownership.
9. El formato final de `TraceEvent`.
10. La integración con ACPI más allá del descubrimiento inicial de RSDP.
11. La forma de cargar módulos desde filesystem.
12. Cuándo y cómo introducir planificación preventiva.
13. SMP y estructuras por CPU.
14. El valor real de páginas grandes y metadata agrupada después de medir.

Estas decisiones deben resolverse cuando exista una necesidad concreta, una prueba o una medición que reduzca la incertidumbre.

---

# 16. Criterio de arquitectura

Una decisión de diseño es aceptable cuando:

```text
es explícita
es pequeña
es comprobable
mantiene ownership claro
no inventa requisitos futuros
puede ser reemplazada sin arrastrar todo el sistema
```

Una abstracción debe justificarse por el problema que resuelve, no por la posibilidad de que algún día sea útil.

La infraestructura central debe permanecer sencilla y semánticamente limitada. Los consumidores deben ser responsables de interpretar los datos que reciben.

Ese criterio es deliberadamente más importante que utilizar una tecnología, patrón o estructura concreta.

---

# 17. Fuentes e inspiración

* The Linux Programming Interface -- Michael Kerrisk
* C Interfaces and Implementations -- David R. Hanson
* Computer Systems: A Programmer's Perspective -- Randal E. Bryant, David R. O'Hallaron
* Linux Kernel Development -- Robert Love
* The Little OS Book
* Operating Systems: Internals and Design Principles
* Operating Systems: Design and Implementation -- Andrew S. Tanenbaum, Albert S. Woodhull
* What Every Computer Scientist Should Know About Floating-Point Arithmetic
* Writing Efficient Programs -- Jon Bentley

Las fuentes son referencias para estudiar mecanismos y decisiones, no especificaciones que Evilos deba copiar.
