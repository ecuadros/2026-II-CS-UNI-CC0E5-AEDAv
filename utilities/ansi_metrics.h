/**
 * @file ansi_metrics.h
 * @ingroup visualization
 * @brief Métricas de texto de terminal (ancho visual UTF-8 ignorando secuencias de escape ANSI).
 */

#ifndef __ANSIMETRICS_H__
#define __ANSIMETRICS_H__

#include <cstddef>
#include <string_view>

namespace Ansi {

/**
 * @brief Calcula el ancho visual en columnas de terminal ignorando secuencias ANSI y contando codepoints UTF-8.
 * @param s Cadena de entrada que puede contener códigos de escape ANSI SGR y caracteres multibyte UTF-8.
 * @return Cantidad de caracteres visibles en pantalla.
 */
inline size_t visible_length(std::string_view s) {
    size_t count = 0, i = 0;
    while (i < s.size()) {
        if (s[i] == '\033' || s[i] == '\x1B') {
            i++;
            if (i < s.size() && s[i] == '[') {
                i++;
                while (i < s.size() && (s[i] < '@' || s[i] > '~')) i++;
                if (i < s.size()) i++;
            }
            continue;
        }
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0x80) == 0) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        count++;
    }
    return count;
}

} // namespace Ansi

using Ansi::visible_length;

#endif // __ANSIMETRICS_H__
