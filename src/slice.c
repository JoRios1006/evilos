#include "slice.h"
#include "string.h" // Para memcmp y strlen tradicionales

StringView sv_from_cstr(const char *str) {
    if (!str) return (StringView){ .data = NULL, .length = 0 };
    return (StringView){ .data = str, .length = strlen(str) };
}

bool sv_equals(StringView a, StringView b) {
    if (a.length != b.length) return false;
    if (a.data == b.data) return true; // Misma referencia en memoria
    if (!a.data || !b.data) return false;
    
    // Usamos memcmp porque sabemos exactamente cuántos bytes comparar
    return memcmp(a.data, b.data, a.length) == 0;
}

bool sv_starts_with(StringView sv, StringView prefix) {
    if (prefix.length > sv.length) return false;
    if (prefix.length == 0) return true;
    
    return memcmp(sv.data, prefix.data, prefix.length) == 0;
}

StringView sv_substr(StringView sv, size_t start, size_t len) {
    if (start >= sv.length) {
        return (StringView){ .data = NULL, .length = 0 };
    }
    
    size_t actual_len = len;
    if (start + len > sv.length) {
        actual_len = sv.length - start; // Limitar al final de la cadena
    }
    
    return (StringView){ .data = sv.data + start, .length = actual_len };
}

StringView sv_split_once(StringView sv, char delimiter, StringView *left) {
    for (size_t i = 0; i < sv.length; i++) {
        if (sv.data[i] == delimiter) {
            *left = (StringView){ .data = sv.data, .length = i };
            return (StringView){ .data = sv.data + i + 1, .length = sv.length - i - 1 };
        }
    }
    
    // No se encontró el delimitador
    *left = sv;
    return (StringView){ .data = NULL, .length = 0 };
}
