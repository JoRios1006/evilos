# Informe Histórico de Depuración e Integración de uACPI en Evilos

Este documento recopila de manera estructurada todos los obstáculos técnicos, errores de compilación, fallos de enlazador y desajustes de configuración que surgieron durante la incorporación de la biblioteca uACPI al kernel de Evilos, detallando las soluciones arquitectónicas aplicadas en cada etapa.

---

## 1. Desajustes en el Sistema de Construcción y Compilador Host

### Problema inicial
Durante la configuración inicial de dependencias externas mediante `FetchContent`, los scripts de CMake de algunas bibliotecas intentaban por defecto utilizar las herramientas del host (GCC/GNU ld) en lugar de la cadena de herramientas cruzada de Evilos (`x86_64-unknown-none-elf-clang` / `ld.lld`), lo que corrompía la caché e intentaba incluir cabeceras estándar del sistema operativo anfitrión.

### Solución aplicada
* Se forzó estrictamente la selección de Clang y LLD **antes** de la invocación de `project()` en el `CMakeLists.txt`.
* Se estableció un perfil riguroso de compilación independiente del host (`-ffreestanding`, `-fno-builtin`, `-mcmodel=kernel`, entre otros).

---

## 2. Advertencias Estrictas del Compilador y Colisión de Símbolos (`-Wpedantic`)

### Problema
Al compilar las fuentes de uACPI bajo un entorno sin cabeceras estándar, Clang generó múltiples advertencias sobre extensiones GNU en macros (`token pasting of ',' and __VA_ARGS__`) y validaciones estrictas de formato. Asimismo, se produjo una colisión de redefinición por un conflicto en el nombre de la función `uacpi_strcmp` implementada estáticamente en el archivo `uacpi_libc.h`.

### Solución aplicada
* Se relajaron selectivamente los flags de error para evitar que el ruido de macros externas detuviera la compilación (`-Wno-error=pedantic`, `-Wno-gnu-zero-variadic-macro-arguments`, `-Wno-format-pedantic`).
* Se renombró la función interna de comparación a `evilos_strcmp` y se redefinió explícitamente el macro `uacpi_strcmp` para evitar que uACPI intentara compilar su propia versión estándar colisionante.

---

## 3. Confusión de Modos de Compilación (`UACPI_BUILD_BAREBONES` vs `UACPI_BAREBONES_MODE`)

### Problema
Este fue uno de los problemas más sutiles y recurrentes. En el archivo `CMakeLists.txt` se había definido el macro `UACPI_BUILD_BAREBONES=1`. Sin embargo, el código fuente de uACPI valida internamente la macro exacta `UACPI_BAREBONES_MODE`. 
Como consecuencia de este desajuste de preprocesador:
1. uACPI asumió que se estaba compilando el **intérprete AML completo** en lugar del modo ligero de tablas (*Barebones*).
2. El enlazador (`ld.lld`) comenzó a exigir más de 40 símbolos y primitivas del sistema operativo que no correspondían a una fase temprana de arranque (como semáforos, mutexes, spinlocks, manejadores de interrupciones, subsistemas PCI y colas de trabajo).

### Solución aplicada
* Se corrigió la definición en el `CMakeLists.txt` utilizando estrictamente el nombre reconocido por los headers de la biblioteca: `UACPI_BAREBONES_MODE=1`.

---

## 4. Inclusión Parcial vs. Completa de Fuentes de uACPI

### Problema
Para mitigar los errores de símbolos indefinidos del punto anterior, inicialmente se intentó recortar manualmente la lista de archivos `.c` (añadiendo únicamente `tables.c`, `types.c` y `stdlib.c`). Esto provocó nuevos errores de enlazador porque uACPI es un sistema de subsistemas interdependientes que requiere obligatoriamente el contexto global (`g_uacpi_rt_ctx`) y las rutinas de inicialización que residen en `uacpi.c` y otros archivos del manifiesto.

### Solución aplicada
* Se descartó la selección manual de fuentes aisladas y se adoptó el mecanismo oficial del repositorio integrando el script `include(${uacpi_SOURCE_DIR}/uacpi.cmake)`.
* Esto permitió consumir automáticamente las variables oficiales `${UACPI_SOURCES}` y `${UACPI_INCLUDES}`, garantizando que todas las dependencias estructurales del modo de tablas estuvieran presentes y correctamente filtradas por el preprocesador.

---

## 5. Firma Incorrecta del Hook `uacpi_kernel_get_rsdp`

### Problema
En el archivo de puente del kernel (`src/uacpi_hooks/kernel_api.c`), la función `uacpi_kernel_get_rsdp` fue escrita inicialmente devolviendo directamente la dirección física como un valor de retorno (`uacpi_phys_addr`). 
No obstante, la interfaz oficial de uACPI exige que esta función reciba un puntero de salida por referencia (`uacpi_phys_addr *out_rsdp_address`) y devuelva un código de estado de tipo `uacpi_status`.

### Solución aplicada
* Se actualizó la firma y la lógica de la función en `kernel_api.c` para cumplir estrictamente con el contrato de la API:
  ```c
  uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
      if (rsdp_req.response == NULL || hhdm_req.response == NULL) {
          return UACPI_STATUS_NOT_FOUND;
      }
      uintptr_t virtual_rsdp = (uintptr_t)rsdp_req.response->address;
      *out_rsdp_address = (uacpi_phys_addr)(virtual_rsdp - hhdm_req.response->offset);
      return UACPI_STATUS_OK;
  }
  ```

---

## 6. Sincronización de la Caché del Sistema de Construcción (Ninja/CMake)

### Problema
Tras múltiples modificaciones estructurales en los archivos de compilación, macros y directorios temporales, el comando `ninja -C build run` fallaba ocasionalmente con el error `loading 'build.ninja': No such file or directory` debido a que la carpeta `build` había sido purgada por completo (`rm -rf build`).

### Solución aplicada
* Se estableció la secuencia correcta de inicialización y ejecución del entorno:
  ```bash
  rm -rf build
  cmake -B build -G Ninja
  ninja -C build run
  ```
* Adicionalmente, se reincorporaron al final del `CMakeLists.txt` los objetivos personalizados de generación de imagen ISO (utilizando `xorriso` y los binarios de Limine) y el lanzamiento automatizado de la máquina virtual en QEMU.

---

## Resumen del Estado Actual
Con estas correcciones, Evilos cuenta con un proceso de compilación limpio, un sistema de enlace robusto que integra uACPI en modo *Barebones* de manera eficiente, y un flujo de desarrollo totalmente automatizado mediante Ninja y QEMU.