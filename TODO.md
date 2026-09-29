# Roadmap de implementación

## 0. Objetivo del roadmap

Construir el sistema desde cero, empezando por un kernel mínimo que pueda arrancar, escribir en pantalla y responder a teclado, y terminar en un sistema experimental en el que Lua gestione gran parte de la lógica de alto nivel.

El orden está diseñado para:

* mantener una versión arrancable durante todo el desarrollo;
* introducir una sola dependencia importante cada vez;
* poder sustituir la gestión inicial de memoria posteriormente;
* evitar implementar filesystem, red o SMP antes de que sean necesarios;
* obtener una demostración útil lo antes posible.

La implementación se divide en cuatro niveles:

```text
Nivel 1
Arranque y kernel mínimo

Nivel 2
Memoria, entrada y gráficos

Nivel 3
Lua, interfaz y planificación

Nivel 4
Memoria completa, módulos, filesystem y red
```

Hay una quinta línea de trabajo opcional:

```text
Nivel 5
Compartición de memoria, páginas grandes y paralelismo
```

No forma parte del primer sistema funcional.

---

# 1. Fase 0: infraestructura del proyecto

## Objetivo

Poder compilar, enlazar, generar una imagen arrancable y ejecutarla repetidamente en QEMU o hardware real.

## Tareas

### Toolchain

* [ ] Elegir y fijar compilador C.
* [ ] Fijar assembler.
* [ ] Fijar linker.
* [ ] Establecer compilación freestanding.
* [ ] Desactivar dependencias accidentales de libc.
* [ ] Establecer flags de compilación para x86-64.
* [ ] Establecer flags de warnings estrictos.
* [ ] Separar flags de debug y release.
* [ ] Comprobar que el kernel no depende de código de userland.

### Link

* [ ] Crear linker script.
* [ ] Definir sección de código.
* [ ] Definir sección de datos.
* [ ] Definir BSS.
* [ ] Exportar símbolos necesarios para localizar el kernel en memoria.
* [ ] Verificar ELF resultante.
* [ ] Comprobar alineamiento de las secciones.

### Limine

* [ ] Fijar versión de Limine utilizada.
* [ ] Crear configuración mínima.
* [ ] Crear imagen arrancable.
* [ ] Arrancar desde QEMU.
* [ ] Arrancar desde hardware real cuando sea posible.
* [ ] Documentar exactamente qué deja preparado Limine.
* [ ] Registrar qué solicitudes del protocolo utiliza el kernel.

### Debug

* [ ] Crear salida de diagnóstico temprana.
* [ ] Poder imprimir texto antes de tener framebuffer.
* [ ] Crear función de panic.
* [ ] Crear manejo de excepciones fatal.
* [ ] Imprimir al menos:

  * [ ] excepción;
  * [ ] código de error;
  * [ ] RIP;
  * [ ] RSP;
  * [ ] CR2 cuando corresponda.

### Build reproducible

* [ ] Un solo comando debe producir la imagen arrancable.
* [ ] Un solo comando debe arrancar QEMU.
* [ ] Un comando debe limpiar artefactos.
* [ ] Documentar dependencias externas.

## Terminado cuando

```text
build
  ↓
imagen
  ↓
QEMU
  ↓
kernel
  ↓
mensaje de arranque
```

funciona siempre.

---

# 2. Fase 1: entrada del kernel y estado inicial

## Objetivo

Entender exactamente en qué estado recibe el control el kernel.

No intentar todavía implementar el sistema de memoria completo.

## Tareas

### Entrada

* [ ] Crear punto de entrada del kernel.
* [ ] Crear stack inicial.
* [ ] Saltar a C.
* [ ] Verificar que las variables globales funcionan.
* [ ] Verificar que BSS está correctamente inicializado.

### Limine

* [ ] Leer memory map.
* [ ] Imprimir todas sus entradas.
* [ ] Identificar memoria usable.
* [ ] Identificar memoria reservada.
* [ ] Identificar la región ocupada por el kernel.
* [ ] Identificar framebuffer.
* [ ] Registrar tamaño y formato del framebuffer.
* [ ] Obtener RSDP y conservar la información para más adelante.
* [ ] Decidir si se solicita también un mecanismo para acceder fácilmente a memoria física.

Esta última decisión es importante antes del VMM. No hace falta resolverla ahora, pero sí dejarla explícita.

### CPU

* [ ] Verificar modo de ejecución.
* [ ] Verificar CPUID disponible.
* [ ] Registrar características básicas de CPU.
* [ ] No depender todavía de extensiones opcionales.

## Terminado cuando

El kernel puede imprimir:

```text
CPU
memory map
kernel location
framebuffer
RSDP
```

y vuelve a un estado de espera sin corromperse.

---

# 3. Fase 2: excepciones e interrupciones

## Objetivo

Tener control explícito sobre los eventos de CPU.

## Tareas

### GDT

* [ ] Crear GDT propia.
* [ ] Definir código de kernel.
* [ ] Definir datos de kernel.
* [ ] Recargar registros correspondientes.
* [ ] Verificar que la ejecución continúa.

### IDT

* [ ] Crear IDT.
* [ ] Registrar excepciones CPU.
* [ ] Crear handlers para:

  * [ ] divide error;
  * [ ] invalid opcode;
  * [ ] general protection;
  * [ ] page fault;
  * [ ] double fault.
* [ ] Imprimir contexto de excepción.
* [ ] Detener el CPU de forma controlada después de un error fatal.

### TSS

* [ ] Crear TSS.
* [ ] Configurar stack de excepción cuando sea necesario.
* [ ] Preparar una ruta segura para double fault.

### Interrupciones externas

* [ ] Inicializar controlador de interrupciones.
* [ ] Habilitar interrupciones explícitamente.
* [ ] Registrar una interrupción de prueba.
* [ ] Crear contador de interrupciones.

## Terminado cuando

Una interrupción entra en C, se registra y retorna correctamente.

Una excepción deliberadamente provocada produce un diagnóstico útil.

---

# 4. Fase 3: memoria inicial

## Objetivo

Conseguir memoria dinámica sin implementar todavía PMM/VMM/TLSF.

Esta fase existe para poder desarrollar lo demás.

## Tareas

### Reservas iniciales

* [ ] Determinar regiones que nunca pueden reutilizarse.
* [ ] Reservar memoria para estructuras del kernel.
* [ ] Reservar memoria para framebuffer secundario.
* [ ] Crear un mecanismo simple de asignación.

El mecanismo inicial puede ser deliberadamente estúpido.

Por ejemplo:

```text
memoria inicial
      ↓
puntero
      ↓
avanzar
      ↓
nueva reserva
```

No necesita `free`.

### Pruebas

* [ ] Reservar 1 byte.
* [ ] Reservar 4 KiB.
* [ ] Reservar un bloque grande.
* [ ] Verificar alineamiento.
* [ ] Escribir y leer memoria.
* [ ] Intentar agotar el espacio disponible.
* [ ] Informar correctamente de la falta de memoria.

### Separación

* [ ] Ocultar la implementación detrás de una interfaz pequeña.
* [ ] No permitir que Lua conozca cómo funciona.
* [ ] No permitir que la interfaz gráfica conozca cómo funciona.
* [ ] Preparar el reemplazo futuro.

## Terminado cuando

El kernel puede solicitar memoria dinámica sin depender de libc.

---

# 5. Fase 4: framebuffer y dibujo mínimo

## Objetivo

Conseguir una salida gráfica estable.

Esta fase produce la primera recompensa visual del proyecto.

## Tareas

### Framebuffer

* [ ] Leer dirección.
* [ ] Leer pitch.
* [ ] Leer ancho.
* [ ] Leer alto.
* [ ] Leer profundidad/formato.
* [ ] Crear una función para escribir un píxel correctamente.
* [ ] Probar todos los extremos de la pantalla.
* [ ] Dibujar un patrón de prueba.

### Buffer secundario

* [ ] Reservar memoria para un buffer del mismo tamaño lógico.
* [ ] Dibujar únicamente en el buffer secundario.
* [ ] Copiar el buffer al framebuffer.
* [ ] Medir tiempo de copia.
* [ ] Verificar que no se producen errores al cambiar resolución.

### Pruebas

* [ ] píxel;
* [ ] línea;
* [ ] rectángulo;
* [ ] pantalla completa;
* [ ] texto bitmap mínimo.

## Terminado cuando

El sistema arranca y muestra una pantalla gráfica conocida sin utilizar todavía Lua.

---

# 6. Fase 5: entrada de teclado

## Objetivo

Obtener eventos de teclado de forma controlada.

## Tareas

### Hardware

* [ ] Inicializar controlador de teclado.
* [ ] Recibir una tecla.
* [ ] Decodificar scancode.
* [ ] Diferenciar pulsación y liberación.
* [ ] Mantener estado de Shift.
* [ ] Mantener estado de Ctrl.
* [ ] Mantener estado de Alt.
* [ ] Manejar Enter.
* [ ] Manejar Backspace.
* [ ] Manejar Escape.

### Eventos

* [ ] Definir representación de un evento de teclado.
* [ ] Separar recepción de hardware y procesamiento.
* [ ] Crear cola de eventos.
* [ ] Probar overflow de la cola.

### Depuración

* [ ] Mostrar eventos recibidos en pantalla.
* [ ] Verificar todas las teclas utilizadas por la interfaz.

## Terminado cuando

Se puede escribir texto en la pantalla utilizando exclusivamente eventos generados por el teclado.

---

# 7. Fase 6: reloj y tiempo

## Objetivo

Tener una fuente de tiempo antes de implementar planificación.

## Tareas

* [ ] Inicializar timer.
* [ ] Generar ticks.
* [ ] Incrementar contador monotónico.
* [ ] Exponer lectura del tiempo.
* [ ] Medir intervalos.
* [ ] Crear espera básica.
* [ ] Verificar que una interrupción periódica funciona.

No implementar todavía un scheduler.

## Terminado cuando

El kernel puede decir:

```text
tick = N
```

y el valor avanza correctamente.

---

# 8. Fase 7: libc mínima para código freestanding

## Objetivo

Permitir compilar componentes complejos sin introducir una libc de sistema completa.

## Tareas

* [ ] Compilar el código que deberá ejecutarse en el kernel.
* [ ] Registrar símbolos faltantes.
* [ ] Implementar únicamente las funciones realmente necesarias.
* [ ] Revisar:

  * [ ] memcpy;
  * [ ] memmove;
  * [ ] memset;
  * [ ] memcmp;
  * [ ] strlen;
  * [ ] funciones de strings necesarias;
  * [ ] operaciones numéricas necesarias.
* [ ] Evitar implementar funciones que todavía nadie utiliza.

### Regla

Cuando aparezca una dependencia:

```text
undefined reference
       ↓
¿realmente se necesita?
       ↓
sí → implementar
no → eliminar dependencia
```

No construir una libc entera porque el linker te haya mirado feo.

## Terminado cuando

Los componentes necesarios para Lua pueden enlazarse sin depender de un sistema operativo existente.

---

# 9. Fase 8: integrar Lua mínimo

## Objetivo

Arrancar Lua dentro del kernel.

Esta es la primera gran integración vertical.

## Tareas

### Incorporación

* [ ] Incorporar el código fuente de Lua.
* [ ] Configurar compilación freestanding.
* [ ] Eliminar dependencias del host que no sean apropiadas.
* [ ] Resolver funciones C requeridas.
* [ ] Crear estado Lua.

### Memoria

* [ ] Conectar allocator de Lua a la memoria inicial.
* [ ] Probar alloc.
* [ ] Probar free.
* [ ] Probar realloc.
* [ ] Agotar memoria deliberadamente.
* [ ] Comprobar que Lua informa el fallo.

### Código

* [ ] Ejecutar una expresión sencilla.
* [ ] Ejecutar un archivo o bloque embebido.
* [ ] Obtener resultados.
* [ ] Mostrar errores Lua.

### Biblioteca estándar

* [ ] Identificar qué partes de la biblioteca estándar son utilizables.
* [ ] Abrir solamente las bibliotecas necesarias.
* [ ] Evitar dependencias accidentales de `io`, `os` y equivalentes que dependan del host.

## Terminado cuando

El kernel hace:

```text
crear lua_State
     ↓
ejecutar código Lua
     ↓
obtener resultado
     ↓
mostrar resultado
```

---

# 10. Fase 9: frontera C ↔ Lua

## Objetivo

Permitir que Lua utilice mecanismos del kernel.

## Tareas

### Primeras funciones

Exponer unas pocas operaciones:

* [ ] dibujar píxel;
* [ ] dibujar primitivas;
* [ ] leer tiempo;
* [ ] obtener evento;
* [ ] imprimir texto;
* [ ] provocar una espera;
* [ ] consultar memoria.

No exponer todavía estructuras internas del kernel.

### Pruebas

* [ ] Lua dibuja un píxel.
* [ ] Lua dibuja un rectángulo.
* [ ] Lua lee el tiempo.
* [ ] Lua recibe un evento.
* [ ] Lua imprime texto.

### Errores

* [ ] Probar argumentos inválidos.
* [ ] Probar número incorrecto de argumentos.
* [ ] Probar punteros inválidos en APIs C.
* [ ] Definir qué errores regresan a Lua.
* [ ] No ejecutar operaciones peligrosas desde una función C sin validación.

## Terminado cuando

Una parte visible del sistema puede escribirse en Lua.

---

# 11. Fase 10: µGUI

## Objetivo

Mover las primitivas gráficas superiores fuera del código gráfico propio.

## Tareas

* [ ] Integrar la biblioteca gráfica.
* [ ] Conectar su salida de píxel.
* [ ] Ejecutar pruebas independientes de sus primitivas.
* [ ] Dibujar líneas.
* [ ] Dibujar rectángulos.
* [ ] Dibujar texto.
* [ ] Dibujar elementos desde Lua.

### Separación

La biblioteca no debe saber:

```text
qué es una ventana
qué es un buffer
qué es el foco
qué es el teclado
```

Solo debe dibujar.

## Terminado cuando

Lua puede producir una interfaz estática completa sin necesidad de modificar el código C gráfico.

---

# 12. Fase 11: buffers e interfaz

## Objetivo

Construir la interfaz descrita en el RFC.

## Tareas

### Buffer

* [ ] Definir qué representa un buffer.
* [ ] Crear uno.
* [ ] Dibujar su contenido.
* [ ] Asignarle una región.
* [ ] Redibujarlo.

### División

* [ ] Dividir pantalla en regiones.
* [ ] Dibujar divisores.
* [ ] Permitir múltiples regiones.
* [ ] Cambiar el tamaño de una región.
* [ ] Redibujar después del cambio.

### Foco

* [ ] Mantener buffer activo.
* [ ] Cambiar foco.
* [ ] Asociar teclado con buffer activo.

### Teclas

* [ ] h
* [ ] j
* [ ] k
* [ ] l
* [ ] Escape
* [ ] :
* [ ] Enter
* [ ] Backspace

## Terminado cuando

Existe una interfaz utilizable aunque todavía no tenga filesystem ni red.

---

# 13. Fase 12: minibuffer y evaluación Lua

## Objetivo

Poder modificar el sistema sin recompilar.

## Tareas

* [ ] Reservar región inferior.
* [ ] Implementar línea de entrada.
* [ ] Implementar edición básica.
* [ ] Leer comando.
* [ ] Pasar string al parser Lua.
* [ ] Ejecutar código.
* [ ] Capturar error.
* [ ] Mostrar resultado.
* [ ] Mantener el sistema ejecutándose después de un error.

### Pruebas

Ejecutar desde el minibuffer operaciones como:

```lua
print(...)
```

y

```lua
some_function()
```

además de modificar variables visibles en la interfaz.

## Terminado cuando

El sistema permite observar y cambiar su estado mediante Lua durante la ejecución.

---

# 14. Fase 13: planificación cooperativa

## Objetivo

Ejecutar varios componentes Lua sin requerir todavía multitarea preventiva.

## Tareas

### Coroutines

* [ ] Crear una coroutine.
* [ ] Reanudarla.
* [ ] Hacer yield.
* [ ] Detectar terminación.
* [ ] Detectar error.

### Scheduler

* [ ] Mantener lista de tareas.
* [ ] Seleccionar siguiente tarea.
* [ ] Reanudar tarea.
* [ ] Detectar yield.
* [ ] Reprogramar tarea.
* [ ] Eliminar tarea terminada.

### Integración

Crear tareas para:

* [ ] teclado;
* [ ] interfaz;
* [ ] renderizado;
* [ ] shell.

### Timer

* [ ] Asociar espera con ticks.
* [ ] Despertar tareas.
* [ ] No ejecutar Lua desde una ISR.

## Prueba importante

Crear:

```lua
taskA()
taskB()
taskC()
```

y verificar que todas progresan.

Después crear deliberadamente:

```lua
while true do
end
```

y comprobar que el sistema efectivamente queda bloqueado.

Eso no es un bug que haya que "arreglar" todavía. Es una propiedad del modelo cooperativo que hay que documentar.

## Terminado cuando

Múltiples tareas Lua funcionan y ceden voluntariamente el control.

---

# 15. Fase 14: PMM básico

Hasta aquí el sistema ya es una demo.

Ahora empieza la segunda etapa del proyecto: reemplazar progresivamente las soluciones temporales.

## Objetivo

Administrar páginas físicas reales.

## Tareas

### Memory map

* [ ] Parsear todas las regiones.
* [ ] Ignorar regiones reservadas.
* [ ] Reservar memoria ocupada por kernel.
* [ ] Reservar memoria usada por módulos.
* [ ] Reservar estructuras iniciales.
* [ ] Reservar framebuffer.
* [ ] Identificar regiones recuperables posteriormente.

### Páginas

* [ ] Elegir unidad básica.
* [ ] Crear estado libre/ocupado.
* [ ] Implementar asignación.
* [ ] Implementar liberación.
* [ ] Probar agotamiento.
* [ ] Probar reutilización.

### Instrumentación

* [ ] Contar páginas libres.
* [ ] Contar páginas ocupadas.
* [ ] Medir fragmentación.
* [ ] Medir coste de alloc/free.

## Terminado cuando

El kernel puede pedir y devolver páginas físicas independientemente de la implementación inicial.

---

# 16. Fase 15: VMM

## Objetivo

Separar memoria virtual de memoria física.

## Tareas

### Page tables

* [ ] Crear estructuras propias.
* [ ] Mapear una página.
* [ ] Desmapear una página.
* [ ] Cambiar permisos.
* [ ] Provocar page fault deliberado.
* [ ] Interpretar CR2.
* [ ] Liberar mapping.

### Regiones

* [ ] Reservar rango virtual.
* [ ] Mapear memoria física.
* [ ] Liberar rango.
* [ ] Permitir regiones no físicamente contiguas.

### Primera integración

Reemplazar progresivamente la memoria inicial del kernel.

No migrar todo de una vez.

Primero:

```text
alguna estructura
```

después:

```text
Lua
```

y finalmente:

```text
resto del sistema
```

## Terminado cuando

Una región virtual continua puede utilizar páginas físicas independientes.

---

# 17. Fase 16: experimentar con páginas grandes

Esta fase existe específicamente para comprobar tu hipótesis.

No asumir que la hipótesis es correcta.

## Experimento A: metadata por página

Implementar una representación sencilla en la que cada página pequeña tenga la información que necesita.

Medir:

* [ ] memoria utilizada por metadata;
* [ ] tiempo de búsqueda;
* [ ] alloc;
* [ ] free;
* [ ] operaciones de referencia.

## Experimento B: metadata agrupada

Agrupar información en unidades mayores.

Medir exactamente lo mismo.

## Experimento C: mezcla

Permitir:

```text
grupo grande
   |
   +-- páginas pequeñas
   +-- páginas pequeñas
   +-- páginas pequeñas
```

y reservar metadata adicional solamente cuando una unidad necesite propiedades especiales.

## Medir

* [ ] bytes de metadata por GiB administrado;
* [ ] coste de alloc;
* [ ] coste de free;
* [ ] coste de sharing;
* [ ] coste de dividir una región;
* [ ] coste de reconstruir metadata;
* [ ] coste de concurrencia;
* [ ] fragmentación.

## Resultado esperado

No es "hacer páginas grandes".

El resultado esperado es:

```text
hipótesis
   ↓
implementación A
   ↓
medición
   ↓
implementación B
   ↓
medición
   ↓
decisión
```

Puede perfectamente concluir que parte de la hipótesis no compensa.

---

# 18. Fase 17: TLSF

## Objetivo

Agregar asignación eficiente de bloques pequeños.

## Tareas

* [ ] Crear pool.
* [ ] Conectar pool a regiones virtuales.
* [ ] Implementar alloc.
* [ ] Implementar free.
* [ ] Implementar realloc.
* [ ] Probar tamaños pequeños.
* [ ] Probar tamaños grandes.
* [ ] Probar muchos alloc/free.
* [ ] Probar fragmentación.
* [ ] Medir coste.

### Integración Lua

* [ ] Reemplazar allocator temporal de Lua.
* [ ] Verificar todas las operaciones.
* [ ] Forzar agotamiento.
* [ ] Liberar memoria repetidamente.
* [ ] Verificar que no existen fugas.

## Terminado cuando

Lua utiliza:

```text
PMM
  ↓
VMM
  ↓
regiones
  ↓
TLSF
  ↓
lua allocator
```

sin conocer los detalles de ninguna capa.

---

# 19. Fase 18: módulo Lua

## Objetivo

Separar el código Lua en componentes cargables.

## Primera versión

No implementar todavía filesystem.

## Tareas

* [ ] Crear loader para módulos embebidos.
* [ ] Registrar módulos disponibles.
* [ ] Implementar `require`.
* [ ] Cargar módulo.
* [ ] Ejecutarlo una vez.
* [ ] Cachearlo.
* [ ] Detectar módulo inexistente.
* [ ] Detectar error durante carga.
* [ ] Detectar dependencia circular.

### Dividir progresivamente

Mover a módulos:

* [ ] interfaz;
* [ ] teclado;
* [ ] scheduler;
* [ ] shell;
* [ ] memoria;
* [ ] pruebas.

## Terminado cuando

La interfaz y el scheduler no dependen de un único archivo Lua gigante.

---

# 20. Fase 19: filesystem mínimo

## Objetivo

Permitir que los módulos dejen de estar necesariamente embebidos.

No intentar hacer un filesystem completo del mundo.

## Primera etapa

* [ ] Elegir medio inicial.
* [ ] Crear acceso a sectores/bloques.
* [ ] Leer un bloque.
* [ ] Escribir un bloque.
* [ ] Implementar estructura mínima de almacenamiento.
* [ ] Crear archivo.
* [ ] Leer archivo.
* [ ] Escribir archivo.
* [ ] Listar archivos.
* [ ] Eliminar archivo.

### Integración con Lua

* [ ] Loader de módulos desde filesystem.
* [ ] `require()` puede encontrar módulos en almacenamiento.
* [ ] Manejar archivo inexistente.
* [ ] Manejar corrupción.
* [ ] Manejar tamaño incorrecto.

## Terminado cuando

El sistema puede arrancar con módulos Lua almacenados fuera del binario.

---

# 21. Fase 20: manejo de errores de módulos

## Objetivo

Comprobar realmente la propiedad:

```text
error en módulo
    ≠
error del kernel
```

## Tareas

Crear deliberadamente módulos que:

* [ ] llamen `error`;
* [ ] reciban argumentos incorrectos;
* [ ] fallen durante `require`;
* [ ] fallen después de estar cargados;
* [ ] devuelvan datos incorrectos.

Comprobar que:

```text
módulo
   ↓
error protegido
   ↓
mensaje
   ↓
módulo detenido
   ↓
resto del sistema sigue
```

Después probar un fallo en C y documentar la diferencia.

Esto debería formar parte de la documentación, porque es una propiedad importante del diseño.

---

# 22. Fase 21: red

La red debería empezar cuando filesystem, memoria y planificación ya funcionen.

## Orden

### Buffers

* [ ] Representar paquete.
* [ ] Crear buffer.
* [ ] Liberar buffer.
* [ ] Compartir buffer internamente.
* [ ] Evitar copias innecesarias donde no hagan falta.

### Ethernet

* [ ] Driver de NIC de prueba.
* [ ] Recibir trama.
* [ ] Enviar trama.
* [ ] Verificar checksum cuando corresponda.
* [ ] Manejar tamaño máximo.

### Protocolos

Implementar solamente los necesarios, en este orden:

* [ ] Ethernet;
* [ ] ARP;
* [ ] IPv4;
* [ ] ICMP;
* [ ] UDP;
* [ ] TCP, solamente si realmente hace falta.

### Primera aplicación

Antes de construir infraestructura compleja:

* [ ] ping;
* [ ] servidor simple;
* [ ] cliente simple.

Después:

* [ ] HTTP;
* [ ] otros servicios.

---

# 23. Fase 22: compartir memoria y zero-copy

Esta fase no debe comenzar hasta que la representación básica de memoria funcione.

## Objetivo

Comprobar qué partes de la hipótesis de páginas grandes y metadata compartida son realmente útiles.

## Tareas

* [ ] Definir cuándo una región puede ser compartida.
* [ ] Representar referencias.
* [ ] Compartir una página.
* [ ] Hacer que dos consumidores lean la misma memoria.
* [ ] Detectar cuándo ya no quedan consumidores.
* [ ] Liberarla.
* [ ] Medir copia frente a compartir.
* [ ] Aplicarlo a buffers de red.
* [ ] Aplicarlo a buffers gráficos cuando sea razonable.

Después comprobar:

```text
buffer
  ↓
productor
  ↓
consumidor
```

sin copiar el contenido innecesariamente.

---

# 24. Fase 23: paralelismo

No comenzar antes de que el sistema funcione correctamente en un solo CPU.

## Tareas

### CPU adicionales

* [ ] Detectar CPUs.
* [ ] Arrancar CPU adicional.
* [ ] Darle stack.
* [ ] Verificar ejecución independiente.
* [ ] Detener CPU sin usar.

### Estado por CPU

Identificar qué estructuras son:

```text
globales
```

y cuáles deben ser:

```text
por CPU
```

### Memoria

* [ ] Hacer allocations desde CPU diferentes.
* [ ] Detectar accesos simultáneos.
* [ ] Proteger estructuras compartidas.
* [ ] Medir contención.

### Scheduler

* [ ] Ejecutar tareas en distintos CPU.
* [ ] Compartir estado.
* [ ] Mover tareas cuando sea necesario.

## Solamente después

Investigar:

* [ ] pools por CPU;
* [ ] estructuras de metadata agrupadas;
* [ ] asignación local;
* [ ] sharing entre CPU.

---

# 25. Fase 24: mejorar la memoria después de tener datos

Esta fase es deliberadamente posterior.

Aquí se toman las decisiones que durante las primeras fases fueron solamente hipótesis.

## Preguntas

* [ ] ¿Cuánta RAM ocupa realmente metadata?
* [ ] ¿Cuánto cuesta buscar una página?
* [ ] ¿Cuánto cuesta compartirla?
* [ ] ¿Cuánto cuesta liberar una región?
* [ ] ¿Las páginas grandes simplifican algo realmente?
* [ ] ¿Cuánto ayudan?
* [ ] ¿Dónde complican las cosas?
* [ ] ¿Conviene metadata por página?
* [ ] ¿Conviene metadata por grupo?
* [ ] ¿Conviene tener ambas?
* [ ] ¿Qué cambia cuando hay varios CPU?

El resultado debe ser una implementación basada en mediciones, no una decisión tomada porque "Linux también lo hace".

---

# 26. Fase 25: estabilización

## Arranque

* [ ] Arranque repetido miles de veces.
* [ ] Arranque con distintas cantidades de RAM.
* [ ] Arranque con distintos framebuffers.
* [ ] Arranque sin módulos opcionales.

## Memoria

* [ ] Exhaustión de memoria.
* [ ] Muchas allocations.
* [ ] Muchos frees.
* [ ] Fragmentación.
* [ ] page faults.
* [ ] corrupción deliberada.

## Lua

* [ ] error de sintaxis.
* [ ] error de ejecución.
* [ ] falta de memoria.
* [ ] módulo inexistente.
* [ ] módulo corrupto.
* [ ] coroutine terminada.
* [ ] coroutine bloqueada.

## Interfaz

* [ ] teclado rápido.
* [ ] cambio rápido de buffers.
* [ ] resize.
* [ ] redibujado continuo.
* [ ] comandos largos.
* [ ] comandos inválidos.

## Scheduler

* [ ] tarea que termina.
* [ ] tarea que genera error.
* [ ] tarea que nunca yield.
* [ ] muchas tareas.
* [ ] tarea bloqueada esperando evento.

---

# 27. Fase 26: documentación

No dejar esto para el final.

## Documentar

* [ ] cómo compilar;
* [ ] cómo arrancar;
* [ ] estado inicial de CPU;
* [ ] memory map;
* [ ] modelo de memoria;
* [ ] framebuffer;
* [ ] interrupciones;
* [ ] teclado;
* [ ] Lua;
* [ ] API C/Lua;
* [ ] scheduler;
* [ ] módulos;
* [ ] filesystem;
* [ ] red;
* [ ] decisiones descartadas;
* [ ] resultados de experimentos.

Para la memoria, especialmente:

```text
hipótesis
→ implementación
→ medición
→ resultado
→ decisión
```

Guardar los resultados aunque la hipótesis resulte incorrecta.

Eso convierte una optimización fallida en información en lugar de simplemente esconder un cadáver detrás de un commit.

---

# 28. Hitos principales

## Hito 1: kernel arrancable

Debe poder:

```text
Limine
  ↓
C
  ↓
diagnóstico
```

Incluye:

* [ ] build;
* [ ] linker;
* [ ] entrada;
* [ ] memory map;
* [ ] panic;
* [ ] excepciones.

---

## Hito 2: kernel interactivo

Debe poder:

```text
keyboard
   ↓
events
   ↓
screen
```

Incluye:

* [ ] interrupciones;
* [ ] timer;
* [ ] teclado;
* [ ] framebuffer;
* [ ] buffer secundario.

---

## Hito 3: Lua arrancado

Debe poder:

```text
kernel
   ↓
Lua
   ↓
script
   ↓
resultado
```

Incluye:

* [ ] allocator temporal;
* [ ] Lua;
* [ ] ejecución;
* [ ] errores;
* [ ] funciones C.

---

## Hito 4: sistema Lua visible

Debe poder:

```text
Lua
 ├── UI
 ├── keyboard
 ├── shell
 └── scheduler
```

Incluye:

* [ ] µGUI;
* [ ] buffers;
* [ ] foco;
* [ ] minibuffer;
* [ ] evaluación interactiva;
* [ ] coroutines.

Este es el primer punto donde ya tienes una demostración interesante.

---

## Hito 5: memoria real

Debe poder:

```text
PMM
 ↓
VMM
 ↓
TLSF
 ↓
Lua
```

Incluye:

* [ ] PMM;
* [ ] VMM;
* [ ] regiones;
* [ ] TLSF;
* [ ] nuevo allocator de Lua.

---

## Hito 6: sistema extensible

Debe poder:

```text
require()
   ↓
module
   ↓
Lua
```

y el módulo puede cargarse desde memoria o filesystem.

---

## Hito 7: sistema utilizable

Debe poder:

```text
boot
 ↓
UI
 ↓
shell
 ↓
filesystem
 ↓
network
```

sin recompilar el núcleo para cada cambio de lógica Lua.

---

# 29. Dependencias críticas

La ruta más importante es:

```text
Limine
   ↓
entrada C
   ↓
diagnóstico
   ↓
interrupciones
   ↓
memoria inicial
   ↓
framebuffer
   ↓
teclado
   ↓
timer
   ↓
libc mínima
   ↓
Lua
   ↓
C ↔ Lua
   ↓
µGUI
   ↓
buffers
   ↓
minibuffer
   ↓
coroutines
   ↓
scheduler
```

Después:

```text
PMM
   ↓
VMM
   ↓
TLSF
   ↓
Lua allocator definitivo
```

y después:

```text
módulos
   ↓
filesystem
   ↓
red
```

Mientras que:

```text
páginas grandes
metadata agrupada
sharing
zero-copy
SMP
```

son una rama posterior y no bloquean el primer sistema funcional.

---

# 30. Orden de trabajo cotidiano

Cada tarea debería terminar en una de estas condiciones:

```text
funciona
```

o:

```text
falla de forma conocida
```

o:

```text
hipótesis descartada
```

Evitar tareas como:

```text
"trabajar en memoria"
```

y convertirlas en:

```text
[ ] leer memory map
[ ] contar páginas utilizables
[ ] reservar una página
[ ] escribir una página
[ ] liberar una página
```

Lo mismo para Lua:

```text
[ ] crear lua_State
[ ] ejecutar expresión
[ ] capturar error
[ ] registrar función C
[ ] pasar argumento C → Lua
[ ] devolver resultado Lua → C
```

Cada commit debería dejar el sistema en un estado arrancable.

---

# 31. Regla de alcance

Una característica nueva solamente entra si responde a una necesidad concreta de una capa existente.

Ejemplo:

```text
Lua necesita malloc
    ↓
necesitamos allocator

allocator necesita regiones
    ↓
necesitamos memoria virtual

memoria virtual necesita páginas
    ↓
necesitamos PMM
```

Pero:

```text
"algún día podríamos necesitar NUMA"
```

no abre una fase de NUMA.

La arquitectura debe dejar espacio para ese futuro sin implementarlo.

---

# 32. Primera versión que considero completa

La primera versión realmente importante no es la que tiene red.

Es esta:

```text
                Limine
                   |
                   v
              kernel C
             /    |    \
           IRQ   MEM    FB
            |     |      |
         keyboard |   backbuffer
                  |
             memoria inicial
                  |
                  v
                 Lua
              /    |    \
             UI  shell  scheduler
              \    |    /
                coroutines
```

Debe poder:

1. arrancar;
2. mostrar una interfaz;
3. aceptar teclado;
4. ejecutar Lua;
5. ejecutar varias tareas cooperativas;
6. ejecutar comandos desde el minibuffer;
7. recuperar errores normales de Lua;
8. mantenerse activo sin filesystem ni red.

Después de eso, PMM/VMM/TLSF dejan de ser infraestructura abstracta y pasan a sustituir componentes que ya sabes que funcionan.

Ese orden reduce muchísimo el riesgo del proyecto: cuando llegues a las partes difíciles de memoria, ya tendrás una máquina que hace cosas y una batería de pruebas con las que comprobar que no rompiste todo.
