/**
 * @file gotoxy.h
 * @brief Utilidad de posicionamiento directo del cursor en consola mediante secuencias ANSI.
 */

#ifndef __GOTOXY_H__
#define __GOTOXY_H__

#include <iostream>
#include "../types.h"

/**
 * @brief Reserva `height` líneas en el flujo y guarda la esquina superior izquierda como origen `(0, 0)`.
 * @param os Flujo de salida de consola.
 * @param height Cantidad de filas a reservar.
 */
inline void gotoxy_origin(std::ostream& os, TX height) {
    if (height <= 0) return;
    for (TX i = 0; i < height; ++i) os << '\n';
    os << "\033[" << height << "A\033[1G\033[s";
}

/**
 * @brief Posiciona el cursor en la coordenada `(x, y)` relativa al origen guardado (`\033[s`).
 * @param os Flujo de salida de consola.
 * @param x Coordenada horizontal (0-indexada).
 * @param y Coordenada vertical (0-indexada).
 */
inline void gotoxy(std::ostream& os, TX x, TX y) {
    if (x < 0 || y < 0) return;
    os << "\033[u";
    if (y > 0) os << "\033[" << y << "B";
    os << "\033[" << (x + 1) << "G";
}

/**
 * @brief Sobrecarga de `gotoxy` que opera directamente sobre `std::cout`.
 * @param x Coordenada horizontal (0-indexada).
 * @param y Coordenada vertical (0-indexada).
 */
inline void gotoxy(TX x, TX y) {
    gotoxy(std::cout, x, y);
}

#endif // __GOTOXY_H__
