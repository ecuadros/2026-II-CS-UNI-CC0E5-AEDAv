/**
 * @file side_by_side.h
 * @ingroup visualization
 * @brief Compone dos dibujos reutilizando `ConsoleGraphRenderer`.
 *
 * Interpreta los movimientos del cursor y conserva el estilo de cada celda.
 * Alinea los dibujos por ancho visible mediante `visible_length()`.
 */

#ifndef __SIDE_BY_SIDE_H__
#define __SIDE_BY_SIDE_H__

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include "../types.h"
#include "ansi_metrics.h"
#include "render.h"
#include "layouts/visual_graph.h"

/**
 * @brief Interpreta la salida de `ConsoleGraphRenderer` (movimientos de cursor + SGR) y la convierte en líneas.
 *
 * Secuencias soportadas: `\n`, `\033[nA` (subir), `\033[nB` (bajar),
 * `\033[nG` (columna absoluta), `\033[s` / `\033[u` (guardar / restaurar
 * cursor) y `\033[...m` (estilo). Los caracteres UTF-8 ocupan una celda.
 * @param stream Texto producido por el renderizador.
 * @return Líneas sin espacios finales; pueden contener SGR por celda.
 */
inline std::vector<std::string> rasterize_console(const std::string& stream) {
    std::vector<std::vector<std::string>> grid;
    Index row{}, col{}, savedRow{}, savedCol{};
    std::string style;

    auto put = [&](const std::string& glyph) {
        if (grid.size() <= row) grid.resize(row + 1);
        auto& line = grid[row];
        if (line.size() <= col) line.resize(col + 1, " ");
        line[col] = style.empty() ? glyph : style + glyph + "\033[0m";
        ++col;
    };

    Index i{};
    while (i < stream.size()) {
        char c = stream[i];
        if (c == '\n') { ++row; col = 0; ++i; continue; }
        if (c == '\033' && i + 1 < stream.size() && stream[i + 1] == '[') {
            Index j = i + 2;
            std::string params;
            while (j < stream.size() && (stream[j] < '@' || stream[j] > '~')) params += stream[j++];
            if (j >= stream.size()) break;
            char command = stream[j];
            Size n{};
            bool hasNumber = false;
            for (char p : params) {
                if (p < '0' || p > '9') break;
                n = n * 10 + (p - '0');
                hasNumber = true;
            }
            if (!hasNumber) n = 1;
            switch (command) {
                case 'A': row = row >= n ? row - n : Index{}; break;
                case 'B': row += n; break;
                case 'G': col = n > 0 ? n - 1 : Index{}; break;
                case 's': savedRow = row; savedCol = col; break;
                case 'u': row = savedRow; col = savedCol; break;
                case 'm':
                    if (params.empty() || params == "0") style.clear();
                    else style += stream.substr(i, j - i + 1);
                    break;
                default: break;
            }
            i = j + 1;
            continue;
        }
        Size len = 1;
        if      ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        put(stream.substr(i, len));
        i += len;
    }

    std::vector<std::string> lines;
    for (auto& cells : grid) {
        while (!cells.empty() && cells.back() == " ") cells.pop_back();
        std::string line;
        for (const auto& cell : cells) line += cell;
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

/**
 * @brief Renderiza `graph` con `ConsoleGraphRenderer` en memoria y lo devuelve como líneas.
 * @param graph Grafo a dibujar (vacío → sin líneas).
 * @param style Estilo de trazo del renderizador.
 * @return Líneas del dibujo (con SGR por celda).
 */
inline std::vector<std::string> console_lines(const VisualGraph& graph, ConsoleLineStyle style = ConsoleLineStyle::Rounded) {
    std::ostringstream os;
    ConsoleGraphRenderer(os, style).render(graph);
    return rasterize_console(os.str());
}

/** @brief Ancho visible máximo de un bloque de líneas. @param lines Bloque. @return Columnas visibles de la línea más ancha. */
inline Size block_width(const std::vector<std::string>& lines) {
    Size width{};
    for (const auto& line : lines) width = std::max(width, visible_length(line));
    return width;
}

/**
 * @brief Rellena `text` con espacios hasta `width` columnas visibles.
 * @param text Texto (puede contener ANSI).
 * @param width Ancho visible deseado.
 * @return `text` rellenado (sin truncar si ya es más ancho).
 */
inline std::string pad_visible(const std::string& text, Size width) {
    Size len = visible_length(text);
    return len >= width ? text : text + std::string(width - len, ' ');
}

/**
 * @brief Compone dos dibujos con título, lado a lado o (si no caben) uno debajo del otro.
 *
 * Lado a lado: fila de títulos, fila separadora y luego cada línea como
 * `izquierda ┃ derecha`, con la columna izquierda rellenada a su ancho
 * visible. Si `ancho izquierdo + 3 + ancho derecho > maxWidth`, se apilan:
 * título, subrayado y dibujo, una línea en blanco, y el segundo bloque.
 *
 * @param left Grafo izquierdo (p. ej. "Antes").
 * @param leftTitle Título del bloque izquierdo.
 * @param right Grafo derecho (p. ej. "Después").
 * @param rightTitle Título del bloque derecho.
 * @param maxWidth Ancho máximo en columnas (por defecto 120).
 * @param style Estilo de trazo del renderizador.
 * @return Texto listo para imprimir (termina en `\n`).
 */
inline std::string side_by_side(const VisualGraph& left, const std::string& leftTitle,
                                const VisualGraph& right, const std::string& rightTitle,
                                Size maxWidth = 120, ConsoleLineStyle style = ConsoleLineStyle::Rounded) {
    const std::string separator = " ┃ ";
    std::vector<std::string> a = console_lines(left, style);
    std::vector<std::string> b = console_lines(right, style);
    Size wa = std::max(block_width(a), visible_length(leftTitle));
    Size wb = std::max(block_width(b), visible_length(rightTitle));

    std::ostringstream out;
    if (wa + visible_length(separator) + wb > maxWidth) {
        auto block = [&](const std::string& title, const std::vector<std::string>& lines, Size width) {
            out << title << '\n';
            for (Size k{}; k < width; ++k) out << "─";
            out << '\n';
            for (const auto& line : lines) out << line << '\n';
        };
        block(leftTitle, a, wa);
        out << '\n';
        block(rightTitle, b, wb);
        return out.str();
    }

    out << pad_visible(leftTitle, wa) << separator << rightTitle << '\n';
    for (Size k{}; k < wa; ++k) out << "─";
    out << "━╋━";
    for (Size k{}; k < wb; ++k) out << "─";
    out << '\n';
    Size rows = std::max(a.size(), b.size());
    for (Index r{}; r < rows; ++r) {
        std::string l = r < a.size() ? a[r] : std::string();
        std::string m = r < b.size() ? b[r] : std::string();
        out << pad_visible(l, wa) << separator << m << '\n';
    }
    return out.str();
}

#endif // __SIDE_BY_SIDE_H__
