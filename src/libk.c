#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// strlen clásico
size_t strlen(const char *s) {
  size_t len = 0;
  while (s[len])
    len++;
  return len;
}

// Convierte un número a cadena hexadecimal
static void itoa_hex(uint64_t value, char *str) {
  const char *hex_chars = "0123456789ABCDEF";
  char temp[20];
  int i = 0;
  if (value == 0) {
    str[0] = '0';
    str[1] = '\0';
    return;
  }
  while (value > 0) {
    temp[i++] = hex_chars[value % 16];
    value /= 16;
  }
  int j = 0;
  while (i > 0) {
    str[j++] = temp[--i];
  }
  str[j] = '\0';
}

// Convierte un número a cadena decimal (con signo)
static void itoa_dec(int64_t value, char *str) {
  char temp[20];
  int i = 0;
  int is_neg = 0;
  if (value == 0) {
    str[0] = '0';
    str[1] = '\0';
    return;
  }
  if (value < 0) {
    is_neg = 1;
    value = -value;
  }
  while (value > 0) {
    temp[i++] = (value % 10) + '0';
    value /= 10;
  }
  int j = 0;
  if (is_neg)
    str[j++] = '-';
  while (i > 0) {
    str[j++] = temp[--i];
  }
  str[j] = '\0';
}

// El motor de formateo principal
int vsnprintf(char *str, size_t size, const char *format, va_list args) {
  size_t i = 0, j = 0;
  while (format[i] && j < size - 1) {
    if (format[i] == '%') {
      i++;
      if (format[i] == 'd') { // Decimal
        int64_t val = va_arg(args, int);
        char buf[20];
        itoa_dec(val, buf);
        for (int k = 0; buf[k] && j < size - 1; k++)
          str[j++] = buf[k];
      } else if (format[i] == 'x' || format[i] == 'X') { // Hexadecimal
        // Usamos uint64_t siempre para %x para poder imprimir punteros
        // fácilmente
        uint64_t val = va_arg(args, uint64_t);
        char buf[20];
        itoa_hex(val, buf);
        for (int k = 0; buf[k] && j < size - 1; k++)
          str[j++] = buf[k];
      } else if (format[i] == 's') { // Cadenas
        char *val = va_arg(args, char *);
        if (!val)
          val = "(null)";
        for (int k = 0; val[k] && j < size - 1; k++)
          str[j++] = val[k];
      } else if (format[i] == 'c') { // Caracteres
        str[j++] = (char)va_arg(args, int);
      } else if (format[i] == '%') {
        str[j++] = '%';
      }
    } else {
      str[j++] = format[i];
    }
    i++;
  }
  str[j] = '\0';
  return j;
}

// Nuestra versión de snprintf
int snprintf(char *str, size_t size, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = vsnprintf(str, size, format, args);
  va_end(args);
  return ret;
}

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;
  for (size_t i = 0; i < n; i++) {
    p[i] = (uint8_t)c;
  }
  return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s) {
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = n; i != 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }
  return 0;
}
