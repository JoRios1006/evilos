# evilos
Emacs Vi Layered Operating System

# RFC: Arquitectura del sistema experimental

**Estado:** Draft

**Alcance:** Discusión de arquitectura

**Implementación:** Fuera del alcance de este documento

---

## 1. Resumen

Este documento describe la arquitectura de un sistema experimental para x86-64.

El sistema tendrá un núcleo pequeño escrito en C y utilizará Lua como lenguaje de alto nivel para buena parte de la lógica del sistema. El arranque se realizará mediante Limine. La salida gráfica partirá del framebuffer entregado por el bootloader y utilizará un buffer secundario para el renderizado. La gestión de memoria se dividirá entre memoria física, memoria virtual y asignación de bloques.

El objetivo de esta arquitectura no es competir con sistemas operativos generales. Se busca una estructura pequeña, comprensible y modificable, en la que las responsabilidades estén separadas y cada capa haga la menor cantidad de trabajo necesaria.

La arquitectura será incremental. Primero se buscará que cada parte funcione con mecanismos simples. Las optimizaciones y mecanismos más complejos de gestión de memoria se incorporarán posteriormente sin obligar a rediseñar las capas superiores.

---

## 2. Objetivos

El sistema deberá:

* arrancar en x86-64 mediante Limine;
* obtener del entorno de arranque el mapa de memoria y el framebuffer;
* disponer de una forma básica de administrar memoria física;
* disponer de memoria virtual;
* proporcionar asignación dinámica de bloques;
* renderizar una interfaz gráfica básica;
* ejecutar una máquina virtual Lua dentro del núcleo;
* permitir que Lua invoque funciones proporcionadas por C;
* permitir cargar y ejecutar módulos escritos en Lua;
* procesar teclado y eventos mediante un modelo inicialmente cooperativo;
* disponer de una interfaz basada en buffers y una línea de comandos integrada;
* mantener una separación clara entre mecanismos de bajo nivel y lógica escrita en Lua.

---

## 3. Fuera de alcance

Este documento no define:

* una implementación concreta de ningún componente;
* una API pública definitiva;
* nombres definitivos para estructuras, funciones o subsistemas;
* un sistema de archivos concreto;
* un protocolo de red concreto;
* un formato de procesos o ejecutables;
* aislamiento entre procesos de usuario;
* planificación preventiva;
* soporte para arquitecturas distintas de x86-64.

Las decisiones que dependan de mediciones o experimentos posteriores se mantienen abiertas.

---

## 4. Supuestos de arranque

Se utilizará Limine como entorno de arranque. La versión actualmente utilizada en el proyecto es Limine v11.

Antes de ejecutar la primera parte del núcleo se solicitarán los recursos necesarios mediante el protocolo correspondiente.

Como mínimo interesan:

* mapa de memoria física;
* framebuffer;
* información necesaria para acceder posteriormente a ACPI.

El sistema no tratará a Limine como un gestor de dispositivos. El hardware que no sea necesario para el arranque será inicializado por el propio sistema.

La información recibida durante el arranque se considera una descripción del estado inicial del sistema. A partir de ella, las capas posteriores construirán sus propias estructuras.

---

## 5. Principio general de la arquitectura

La arquitectura seguirá una separación simple:

```text
Hardware
    |
    v
C
    |
    v
Lua
    |
    v
Aplicaciones y lógica del sistema
```

El código C será responsable de los mecanismos que requieren acceso directo al hardware, control de memoria, manejo de interrupciones y operaciones que no resulten adecuadas para Lua.

Lua será responsable de la lógica de más alto nivel siempre que sea posible.

La distinción principal será:

```text
C   = acceso y mecanismos
Lua = lógica y coordinación
```

Esto no implica que Lua tenga acceso ilimitado a cualquier recurso del sistema. Las operaciones disponibles desde Lua dependerán explícitamente de las funciones que el núcleo exponga.

---

# 6. Gestión de memoria

## 6.1. Objetivo inicial

La gestión de memoria se desarrollará en etapas.

La primera etapa tendrá como objetivo hacer posible la ejecución del sistema. No se buscará inicialmente una implementación completa o especialmente eficiente.

La segunda etapa podrá reemplazar progresivamente las soluciones iniciales por mecanismos más elaborados sin cambiar la interfaz utilizada por el resto del sistema.

Por ejemplo, el mecanismo utilizado inicialmente para proporcionar memoria a Lua podrá posteriormente pasar a utilizar memoria física, memoria virtual y un asignador de bloques sin que Lua necesite conocer los detalles del cambio.

---

## 6.2. Memoria física

El mapa proporcionado por Limine será la fuente inicial para determinar qué memoria física puede utilizar el sistema.

La unidad básica considerada será la página de 4 KiB.

Se considera conveniente mantener soporte para páginas de mayor tamaño además de las páginas pequeñas.

La razón principal es que las páginas pequeñas proporcionan una granularidad fina, mientras que las páginas grandes pueden reducir la cantidad de estructuras necesarias para representar determinados rangos de memoria y reducir el coste asociado a determinadas operaciones.

La elección del tamaño de página utilizado en un rango determinado no implica que toda la memoria deba ser gestionada de una única manera.

---

## 6.3. Metadata de memoria

Se considera una hipótesis de diseño que las páginas pequeñas podrían no necesitar una estructura de metadata individual equivalente para cada página.

Una posible organización sería asociar parte de la información administrativa a unidades de memoria mayores, mientras que las páginas de 4 KiB podrían permanecer en un estado más simple.

La motivación es reducir la cantidad de información administrativa que debe mantenerse cuando existe una gran cantidad de páginas pequeñas.

Esta idea presenta varias cuestiones abiertas.

Para una página individual puede ser necesario conocer, dependiendo del uso:

* si está libre;
* si está asignada;
* quién es su propietario;
* si está compartida;
* si tiene referencias activas;
* si forma parte de una región mayor;
* si puede ser liberada independientemente;
* si está relacionada con alguna operación de entrada/salida.

No todas estas propiedades necesitan necesariamente estar representadas en cada página.

Una posible consecuencia de asociar metadata a unidades mayores es que determinadas páginas pequeñas compartan información administrativa común. Esto puede reducir memoria utilizada por las estructuras de gestión, pero aumenta la cantidad de trabajo necesaria para representar excepciones.

Por tanto, la siguiente cuestión queda abierta:

```text
¿Cuánta información debe existir por página
y cuánta puede asociarse a grupos de páginas?
```

No se adopta todavía una respuesta.

---

## 6.4. Páginas grandes

El uso de páginas grandes se considera principalmente una herramienta de organización y eficiencia, no una obligación para todos los tipos de memoria.

Puede resultar útil para:

* regiones grandes con una duración relativamente larga;
* buffers grandes;
* estructuras compartidas;
* regiones utilizadas simultáneamente por varios componentes;
* memoria que se quiera gestionar como una unidad.

También podría facilitar una futura organización del metadata por grupos de páginas.

No se asume que una página grande deba permanecer siempre íntegra. La arquitectura deberá permitir, al menos conceptualmente, dividir una región grande cuando sea necesario utilizar partes pequeñas de ella.

---

## 6.5. Compartición de memoria

La arquitectura deberá dejar abierta la posibilidad de compartir páginas entre componentes sin copiar su contenido.

Esto requiere distinguir entre:

```text
memoria que pertenece a una sola entidad
```

y

```text
memoria utilizada por varias entidades
```

La forma concreta de representar las referencias no se define en este RFC.

La gestión de memoria deberá permitir evolucionar hacia este modelo sin introducir desde el principio estructuras innecesarias para un sistema que inicialmente será simple.

La misma consideración se aplica a mecanismos posteriores como copiar al escribir. No forman parte de la primera etapa, pero la representación interna de memoria no debería hacerlos imposibles.

---

## 6.6. Memoria virtual

La memoria virtual se utilizará para separar:

* direcciones virtuales;
* direcciones físicas;
* regiones de memoria;
* asignación de bloques.

La capa de memoria virtual deberá permitir solicitar regiones virtuales de tamaño determinado sin exigir que su memoria física sea contigua.

Esto permite representar una región virtual continua mediante páginas físicas independientes.

Por ejemplo:

```text
Virtual:
+----------------------------------+
|             región               |
+----------------------------------+

Física:
+--------+  +--------+  +--------+
| página |  | página |  | página |
+--------+  +--------+  +--------+
```

La contigüidad virtual no implica contigüidad física.

La posibilidad de utilizar páginas grandes deberá quedar integrada en esta capa, pero no es necesario resolver todas sus reglas en la primera versión.

---

## 6.7. Asignación de bloques

Lua necesita asignar bloques de tamaños muy diferentes.

La memoria virtual y física trabajan naturalmente con páginas, pero las estructuras de alto nivel trabajan con objetos mucho menores.

Por ello habrá una separación entre:

```text
páginas
    |
    v
regiones
    |
    v
bloques
```

El asignador de bloques podrá utilizar regiones obtenidas desde la gestión de memoria virtual.

TLSF es el mecanismo considerado para esta función.

La elección concreta de cómo obtener, ampliar y liberar regiones queda abierta.

---

# 7. Framebuffer y representación gráfica

Limine proporcionará un framebuffer lineal.

El núcleo expondrá una operación básica para escribir píxeles en ese framebuffer.

La interfaz gráfica no escribirá directamente en el framebuffer para cada operación de dibujo.

En su lugar se utilizará un segundo buffer de memoria.

El flujo será:

```text
Lua / UI
    |
    v
primitivas gráficas
    |
    v
buffer secundario
    |
    v
framebuffer
```

Las operaciones de dibujo se realizarán sobre el buffer secundario.

Posteriormente, el contenido necesario se copiará al framebuffer.

La frecuencia de actualización no se considera todavía un requisito rígido. Inicialmente podrá utilizarse una frecuencia fija y posteriormente evaluarse si resulta conveniente modificarla.

La representación del framebuffer deberá respetar las características proporcionadas por Limine, incluyendo tamaño de línea, resolución y formato de píxel.

---

# 8. Biblioteca gráfica

Se utilizará una biblioteca gráfica pequeña para evitar reimplementar primitivas básicas de dibujo.

La biblioteca recibirá una operación de bajo nivel para escribir píxeles y utilizará esa operación para construir elementos gráficos superiores.

La responsabilidad de la biblioteca será limitada a:

* primitivas gráficas;
* texto bitmap;
* coordenadas;
* elementos gráficos básicos.

La organización de ventanas, buffers de interfaz y comportamiento de la interfaz no pertenecerá a esta capa.

---

# 9. Lua

## 9.1. Posición de Lua

Lua se ejecutará dentro del espacio de direcciones del núcleo.

El núcleo C iniciará la máquina virtual y proporcionará el mecanismo de memoria que utilizará Lua.

Lua no tendrá acceso directo al hardware. Las operaciones de hardware estarán disponibles mediante funciones registradas desde C.

Por ejemplo, una operación conceptual podrá ser:

```text
Lua
 |
 v
función expuesta por C
 |
 v
driver o mecanismo del núcleo
 |
 v
hardware
```

---

## 9.2. API entre C y Lua

La frontera entre C y Lua deberá ser pequeña.

Cada función expuesta deberá representar una operación concreta del sistema.

La intención es evitar trasladar a Lua estructuras internas innecesarias.

La interfaz deberá distinguir entre:

```text
operaciones que modifican el estado del sistema
```

y

```text
datos que Lua puede inspeccionar
```

La forma definitiva de las funciones queda fuera de este documento.

---

## 9.3. Memoria de Lua

Lua deberá utilizar el sistema general de asignación de memoria del núcleo.

Inicialmente podrá utilizarse un mecanismo sencillo.

Posteriormente, el mismo punto de integración podrá utilizar la memoria administrada mediante regiones y TLSF.

Esto permite que la evolución de la gestión de memoria no obligue a modificar el runtime de Lua.

---

# 10. Módulos

La mayor parte de la lógica de alto nivel se organizará como módulos Lua.

Inicialmente los módulos podrán formar parte de la imagen del sistema. Posteriormente podrán obtenerse de un sistema de archivos.

La interfaz conceptual deberá ser la misma en ambos casos.

Los módulos previstos incluyen, entre otros:

```text
interfaz
entrada
shell
planificación
sistema de archivos
red
```

La lista no es definitiva.

Un error Lua normal deberá poder ser tratado como error del módulo y no como un error irrecuperable del núcleo.

Esto es posible para errores capturados por el runtime de Lua.

No proporciona aislamiento frente a:

* corrupción de memoria;
* errores en código C;
* accesos inválidos realizados desde funciones expuestas a Lua;
* fallos del propio núcleo;
* bucles que no cedan el control.

Por tanto, el sistema no debe asumir que ejecutar un módulo Lua equivale a ejecutar un proceso aislado.

---

# 11. Interfaz de usuario

La interfaz se organizará alrededor de regiones rectangulares de contenido.

No se requiere un sistema de ventanas flotantes en la primera etapa.

El área principal podrá dividirse en varias regiones y cada región podrá contener texto, gráficos u otro contenido.

El cálculo de la distribución podrá realizarse desde Lua.

La capa gráfica solamente deberá recibir las coordenadas y operaciones necesarias para representar el resultado.

La división física de la pantalla y la política utilizada para decidir qué región recibe el foco son problemas distintos y deberán mantenerse separados.

---

# 12. Entrada de teclado

El núcleo recibirá los eventos de entrada procedentes del hardware.

Las interrupciones deberán realizar el trabajo mínimo necesario para registrar el evento.

El procesamiento posterior podrá realizarse fuera del contexto de interrupción.

Una posible secuencia es:

```text
hardware
    |
    v
interrupción
    |
    v
evento
    |
    v
cola
    |
    v
Lua
```

La forma exacta de la cola y del evento queda abierta.

---

# 13. Línea de comandos y ejecución de Lua

La interfaz reservará una región pequeña para mensajes del sistema y entrada de comandos.

El usuario podrá introducir expresiones o código Lua y evaluarlo sin reiniciar el sistema.

La ejecución deberá utilizar el mecanismo protegido del runtime para que un error del código introducido pueda convertirse en un mensaje y no necesariamente en la terminación del runtime.

Esta capacidad será especialmente útil para inspeccionar y modificar el estado del sistema durante el desarrollo.

No se considera necesario que toda operación del sistema sea modificable dinámicamente desde Lua.

---

# 14. Planificación

La primera etapa utilizará planificación cooperativa.

Cada tarea Lua deberá ceder explícitamente el control.

El planificador mantendrá una colección de tareas ejecutables y elegirá cuál continuar.

Una forma conceptual es:

```text
tarea A
    |
    v
resume
    |
    v
yield

tarea B
    |
    v
resume
    |
    v
yield
```

El timer del sistema podrá utilizarse para generar eventos y medir el tiempo.

No se ejecutará código Lua arbitrario dentro del contexto de una interrupción.

La planificación preventiva queda fuera del alcance inicial.

---

# 15. Modelo de fallos

El sistema debe distinguir entre distintos niveles de fallo.

## 15.1. Error Lua

Ejemplo:

```text
módulo
    |
    v
error()
```

Resultado esperado:

```text
error del módulo
```

El resto del sistema puede continuar.

## 15.2. Error en una función C

Ejemplo:

```text
Lua
 |
 v
función C
 |
 v
acceso inválido
```

Resultado posible:

```text
fallo del núcleo
```

Este caso no se considera aislado.

## 15.3. Error del hardware o de la gestión de memoria

Estos errores pertenecen al núcleo y requieren mecanismos propios de manejo.

No se presupone que Lua pueda recuperarse de ellos.

---

# 16. Orden de desarrollo

La arquitectura permite un desarrollo incremental.

Una secuencia razonable es:

```text
arranque
  |
  v
salida mínima
  |
  v
memoria inicial
  |
  v
interrupciones
  |
  v
framebuffer
  |
  v
entrada
  |
  v
Lua
  |
  v
interfaz
  |
  v
planificación cooperativa
  |
  v
módulos
  |
  v
memoria más completa
  |
  v
filesystem
  |
  v
red
```

El orden exacto podrá cambiar según las dependencias observadas durante el desarrollo.

La propiedad importante es que una solución inicial simple pueda sustituirse posteriormente sin cambiar las capas superiores.

---

# 17. Cuestiones abiertas

Las siguientes cuestiones no quedan decididas por este RFC:

1. Cuánta metadata debe existir por página de 4 KiB.
2. Qué información puede asociarse a grupos de páginas.
3. Cómo representar memoria compartida.
4. Cómo dividir una página grande cuando algunas páginas pequeñas dejan de compartir sus propiedades.
5. Cuándo utilizar páginas grandes.
6. Cómo detectar y recuperar regiones que dejen de estar compartidas.
7. Cómo obtener nuevas regiones para el asignador de bloques.
8. Qué operaciones de hardware estarán expuestas a Lua.
9. Cómo se almacenarán inicialmente los módulos Lua.
10. Cuándo será necesario incorporar planificación preventiva.
11. Qué mecanismos de recuperación se utilizarán ante excepciones del núcleo.
12. Cómo se integrará ACPI posteriormente.
13. Qué parte de la interfaz gráfica deberá permanecer en C y qué parte deberá pertenecer a Lua.

Estas cuestiones deberán resolverse únicamente cuando exista una necesidad concreta o evidencia obtenida mediante pruebas.

---

# 18. Principios de diseño

El sistema seguirá unas pocas reglas simples:

* Preferir una solución simple que funcione a una solución general que todavía no sea necesaria.
* Mantener las capas separadas.
* Evitar que las capas superiores dependan de detalles de hardware.
* Evitar duplicar información sin una razón concreta.
* No introducir aislamiento que el sistema todavía no necesita.
* No introducir optimizaciones antes de conocer su coste real.
* Mantener abiertas las partes que todavía no pueden decidirse con información suficiente.
* Permitir que una implementación inicial pueda reemplazarse sin rediseñar todo el sistema.

El objetivo no es definir todas las propiedades del sistema desde el principio. El objetivo es establecer suficientes límites para que las decisiones posteriores no entren en conflicto entre sí.


# FUENTES (INSPIRACION?):
* The Linux Programming Interface -- Michael Kerrisk
* C Interfaces and Implementations -- David R_ Hanson -- 1996
* Computer Systems_ A Programmer’s Perspective 3rd Edition -- Randal E_ Bryant, David R_ O’Hallaron -- 3, Global Edition, 2015 -- Pearson Education
* Linux Kernel Development 3rd Edition
* Little OS Book
* Operating Systems - Internals and Design Principles 7th
* What every computer scientist should know about floating-point arithmetic
* Writing efficient programs (Bentley, Jon Louis)
* Computer Systems A Programmer’s Perspective
* Operating Systems: Design and Implementation, 3rd ed. Andrew S. Tanenbaum, and Albert S. Woodhull 
