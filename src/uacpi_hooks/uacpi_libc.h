#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

// Incluimos tu librería estándar del kernel
#include "string.h"

// Mapeo directo a libk.c
#define uacpi_memcpy   memcpy
#define uacpi_memset   memset
#define uacpi_memcmp   memcmp
#define uacpi_strlen   strlen
#define uacpi_snprintf snprintf
#define uacpi_vsnprintf vsnprintf

// Renombramos la función para que no colisione con el símbolo interno de uACPI
static inline int evilos_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// Declaramos el macro explícito para que el preprocesador de uACPI lo detecte y no compile el suyo
#define uacpi_strcmp evilos_strcmp
