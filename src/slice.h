#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Slice: Una vista inmutable a un búfer binario arbitrario
typedef struct {
    const uint8_t *data;
    size_t length;
} Slice;

// StringView: Una vista inmutable a una cadena de texto (no necesariamente nula-terminada)
typedef struct {
    const char *data;
    size_t length;
} StringView;

// ==========================================
// CONSTRUCTORES
// ==========================================

// Crear StringView desde un literal de C ("Texto") en tiempo de compilación.
// sizeof() incluye el '\0', por lo que restamos 1.
#define SV(literal) ((StringView){ .data = (literal), .length = sizeof(literal) - 1 })

// Crear StringView desde un string C tradicional en tiempo de ejecución (requiere tu clásico strlen)
StringView sv_from_cstr(const char *str);

// ==========================================
// OPERACIONES SEGURAS (Reemplazo de string.h)
// ==========================================

// Reemplazo de strcmp()
bool sv_equals(StringView a, StringView b);

// Comprobar prefijos
bool sv_starts_with(StringView sv, StringView prefix);

// Subcadenas seguras sin copiar (Reemplazo de strstr o aritmética de punteros)
StringView sv_substr(StringView sv, size_t start, size_t len);

// Cortar un StringView basado en un delimitador (Reemplazo de strtok)
// Separa 'sv' en el primer delimitador que encuentre.
// 'left' contendrá lo de la izquierda, y devuelve lo de la derecha para seguir iterando.
StringView sv_split_once(StringView sv, char delimiter, StringView *left);
