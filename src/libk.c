#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "slice.h"

/* Tamaño del buffer temporal para convertir enteros de 64 bits:
 * 20 dígitos (UINT64_MAX en decimal) + signo + NUL = 22 <= 24. */
#define NUM_BUF_SIZE 24U

/* Cursor de salida del formateador.
 * cap = máximo de caracteres a escribir (sin contar el '\0' final). */
typedef struct {
  char *buf;
  size_t cap;
  size_t pos;
} FmtOut;

/* ------------------------------------------------------------------ */
/* strlen                                                             */
/* ------------------------------------------------------------------ */

/* strlen clásico. El contrato de la función exige una cadena terminada en
 * '\0'; ese es el comportamiento estándar y no se puede cambiar. */
size_t strlen(const char *s) { // flawfinder: ignore
  size_t len = 0U;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

/* ------------------------------------------------------------------ */
/* Helpers de formateo                                                */
/* ------------------------------------------------------------------ */

/* Agrega un carácter si queda lugar. */
static void out_char(FmtOut *out, char c) {
  if (out->pos < out->cap) {
    out->buf[out->pos] = c;
    out->pos++;
  }
}

/* Agrega una cadena terminada en '\0' (se trunca si no entra). */
static void out_str(FmtOut *out, const char *s) {
  size_t k = 0U;
  while ((s[k] != '\0') && (out->pos < out->cap)) {
    out->buf[out->pos] = s[k];
    out->pos++;
    k++;
  }
}

/* Agrega exactamente n bytes, ignorando cualquier '\0' intermedio. */
static void out_mem(FmtOut *out, const char *s, size_t n) {
  size_t k = 0U;
  while ((k < n) && (out->pos < out->cap)) {
    out->buf[out->pos] = s[k];
    out->pos++;
    k++;
  }
}

/* Convierte value a texto en la base indicada (10 o 16, mayúsculas).
 * Escribe de atrás hacia adelante dentro de buf y devuelve el índice del
 * primer carácter; buf[NUM_BUF_SIZE - 1] siempre es '\0'. */
static size_t u64_to_buf(uint64_t value, uint64_t base, char *buf) {
  static const char digits[] = "0123456789ABCDEF";
  size_t pos = NUM_BUF_SIZE - 1U;
  uint64_t v = value;
  buf[pos] = '\0';
  do {
    pos--;
    buf[pos] = digits[v % base];
    v /= base;
  } while (v != 0U);
  return pos;
}

/* Decimal con signo. Usa la magnitud sin signo para que INT64_MIN no sea UB. */
static void emit_dec(FmtOut *out, int64_t value) {
  char buf[NUM_BUF_SIZE]; // flawfinder: ignore
  uint64_t mag = (uint64_t)value;
  if (value < 0) {
    mag = UINT64_C(0) - mag;
  }
  size_t start = u64_to_buf(mag, 10U, buf);
  if (value < 0) {
    start--;
    buf[start] = '-';
  }
  out_str(out, &buf[start]);
}

/* Hexadecimal sin prefijo (siempre mayúsculas, igual que antes). */
static void emit_hex(FmtOut *out, uint64_t value) {
  char buf[NUM_BUF_SIZE]; // flawfinder: ignore
  size_t start = u64_to_buf(value, 16U, buf);
  out_str(out, &buf[start]);
}

/* Cadena C; NULL se imprime como "(null)". */
static void emit_cstr(FmtOut *out, const char *s) {
  if (s == NULL) {
    out_str(out, "(null)");
  } else {
    out_str(out, s);
  }
}

/* StringView: se itera hasta sv.length, ignorando cualquier '\0'. */
static void emit_view(FmtOut *out, StringView sv) {
  if (sv.data == NULL) {
    out_str(out, "(null view)");
  } else {
    out_mem(out, sv.data, sv.length);
  }
}

/* ------------------------------------------------------------------ */
/* Motor de formateo (usa <stdarg.h>)                                 */
/* ------------------------------------------------------------------ */

/* Justificación MISRA C:2012 Rule 17.1: un printf de kernel es
 * intrínsecamente variádico; no existe alternativa conforme a la regla. */

// cppcheck-suppress-begin misra-c2012-17.1

/* Procesa un único especificador de conversión. */
static void emit_spec(FmtOut *out, char spec, va_list *args) {
  switch (spec) {
  case 'd': { /* Decimal (int) */
    int64_t val = va_arg(*args, int);
    emit_dec(out, val);
    break;
  }
  case 'x':
  case 'X': { /* Hexadecimal: uint64_t siempre, para imprimir punteros */
    uint64_t val = va_arg(*args, uint64_t);
    emit_hex(out, val);
    break;
  }
  case 's': { /* Cadenas */
    const char *val = va_arg(*args, const char *);
    emit_cstr(out, val);
    break;
  }
  case 'v': { /* StringView */
    StringView sv = va_arg(*args, StringView);
    emit_view(out, sv);
    break;
  }
  case 'c': { /* Caracteres */
    char val = (char)va_arg(*args, int);
    out_char(out, val);
    break;
  }
  case '%':
    out_char(out, '%');
    break;
  default:
    /* Especificador desconocido: se descarta, igual que antes. */
    break;
  }
}

/* Recorre el formato y vuelca el resultado en out. */
static void format_into(FmtOut *out, const char *format, va_list *args) {
  size_t i = 0U;
  while ((format[i] != '\0') && (out->pos < out->cap)) {
    if (format[i] == '%') {
      i++;
      /* Un '%' al final del formato no consume nada ni se sale del string. */
      if (format[i] != '\0') {
        emit_spec(out, format[i], args);
        i++;
      }
    } else {
      out_char(out, format[i]);
      i++;
    }
  }
}

/* El motor de formateo principal.
 * Devuelve la cantidad de caracteres escritos (sin el '\0'). Siempre termina
 * en '\0' si size > 0. Con size == 0 o punteros nulos no escribe nada.
 * El formato lo controla el kernel (nunca llega desde input externo). */
int vsnprintf(char *str, size_t size, const char *format, // flawfinder: ignore
              va_list args) {
  size_t written = 0U;
  if ((str != NULL) && (format != NULL) && (size != 0U)) {
    FmtOut out;
    va_list ap;
    out.buf = str;
    out.cap = size - 1U;
    out.pos = 0U;
    va_copy(ap, args);
    format_into(&out, format, &ap);
    va_end(ap);
    str[out.pos] = '\0';
    written = out.pos;
  }
  return (int)written;
}

/* Nuestra versión de snprintf */
int snprintf(char *str, size_t size, const char *format, // flawfinder: ignore
             ...) {
  va_list args;
  int ret;
  va_start(args, format);
  ret = vsnprintf(str, size, format, args); // flawfinder: ignore
  va_end(args);
  return ret;
}

// cppcheck-suppress-end misra-c2012-17.1

/* ------------------------------------------------------------------ */
/* Funciones de memoria                                               */
/* ------------------------------------------------------------------ */

/* Justificación MISRA C:2012 Rule 11.5: las funciones mem* reciben void* por
 * contrato estándar y necesitan convertirlo a uint8_t* para operar byte a
 * byte. */

// cppcheck-suppress-begin misra-c2012-11.5

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;
  for (size_t i = 0U; i < n; i++) {
    p[i] = (uint8_t)c;
  }
  return s;
}

/* El llamador garantiza que dest tiene al menos n bytes: es el contrato
 * estándar de memcpy y no hay forma de verificarlo desde acá. */
void *memcpy(void *dest, const void *src, size_t n) { // flawfinder: ignore
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0U; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s) {
    for (size_t i = 0U; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = n; i != 0U; i--) {
      d[i - 1U] = s[i - 1U];
    }
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  int result = 0;
  size_t i = 0U;
  while ((result == 0) && (i < n)) {
    if (p1[i] != p2[i]) {
      if (p1[i] < p2[i]) {
        result = -1;
      } else {
        result = 1;
      }
    }
    i++;
  }
  return result;
}

// cppcheck-suppress-end misra-c2012-11.5
