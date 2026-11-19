#ifndef __SPREADSHEET_H__
#define __SPREADSHEET_H__

#include "exprtree.h"
#include "traversalops.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <istream>
#include <limits>
#include <mutex>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/** Error de una celda; es el mismo error que lanza ExprTree. */
using SheetError = ExprError;

/**
 * Hoja de cálculo de texto. Las celdas se guardan en una HashTable por nombre; al pedir una celda, su fórmula se convierte en un ExprTree (AST) y se evalúa recorriéndolo en postorden, por lo que los cambios se reflejan en cascada sin guardar valores.
 * Soporta `+ - * / ^`, paréntesis, `+`/`-` unarios, referencias (`A1`) y las funciones `SUMA`, `PROM`, `MAX`, `MIN` y `CONTAR` con argumentos o rangos (`A1:B3`).
 * Se guarda y se lee con `<<`/`>>` en el formato `[(celda,texto),...]` de la HashTable (mixins PrintableOp/ReadableOp del AVL). Cada operación pública toma el mutex de LockableOp.
 * @tparam Traits Tipos de la hoja: `number_type`, `Cells`, `Formula` y `Sequence`.
 */
template <typename Traits> class Sheet : public LockableOp {
public:
    using Number = typename Traits::number_type;
    using Formula = typename Traits::Formula;

private:
    using Cells = typename Traits::Cells;
    template <typename T> using Sequence = typename Traits::template Sequence<T>;
    using Cache = std::unordered_map<std::string, Number>;

    Cells m_cells;
    /** Celdas que se están evaluando ahora; sirve para detectar ciclos. */
    mutable Sequence<std::string> m_active;

    /** Convierte @p text a número; falla si no es un número completo. */
    static bool to_number(const std::string& text, Number& out) { return Formula::to_number(text, out); }

    /** Indica si el texto de la celda es una fórmula. */
    static bool is_formula(const std::string& source) { return !source.empty() && source[0] == '='; }

    /** Indica si la celda guarda texto que no es fórmula ni número. */
    static bool is_text(const std::string& source) {
        Number ignored;
        return !is_formula(source) && !to_number(source, ignored);
    }

    /** Comprueba que el contenido de la celda no tenga saltos de línea. */
    static void validate_source(const std::string& source) {
        if (source.find('\n') != std::string::npos) throw std::invalid_argument("La celda no admite saltos de linea");
    }

    /** Separa `AB12` en columna (base 26, desde 1) y fila (desde 1); lanza si desborda `int` o la fila es 0. */
    static std::pair<int, int> split(const std::string& name) {
        constexpr int max_int = std::numeric_limits<int>::max();
        int col = 0;
        std::size_t i = 0;
        for (; i < name.size() && std::isalpha(static_cast<unsigned char>(name[i])); ++i) {
            int digit = name[i] - 'A' + 1;
            if (col > (max_int - digit) / 26) throw std::invalid_argument("Nombre de celda invalido: " + name);
            col = col * 26 + digit;
        }
        int row = 0;
        for (; i < name.size(); ++i) {
            int digit = name[i] - '0';
            if (row > (max_int - digit) / 10) throw std::invalid_argument("Nombre de celda invalido: " + name);
            row = row * 10 + digit;
        }
        if (col < 1 || row < 1) throw std::invalid_argument("Nombre de celda invalido: " + name);
        return {col, row};
    }

    /** Pasa el nombre a mayúsculas y comprueba que sea letras seguidas de dígitos con fila >= 1 y sin desbordamiento. */
    static std::string normalize(std::string name) {
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
        std::size_t letters = 0;
        while (letters < name.size() && std::isalpha(static_cast<unsigned char>(name[letters]))) ++letters;
        bool digits = letters < name.size() &&
                      std::all_of(name.begin() + letters, name.end(), [](unsigned char c) { return std::isdigit(c); });
        if (letters == 0 || !digits) throw std::invalid_argument("Nombre de celda invalido: " + name);
        split(name);
        return name;
    }

    /** Letras de la columna @p col (base 26, desde 1). */
    static std::string letters_of(int col) {
        std::string letters;
        for (; col > 0; col = (col - 1) / 26) letters.insert(letters.begin(), static_cast<char>('A' + (col - 1) % 26));
        return letters;
    }

    /** Nombre de la celda en la columna @p col y la fila @p row. */
    static std::string name_of(int col, int row) { return letters_of(col) + std::to_string(row); }

    /** Agrega a @p args las celdas numéricas del rectángulo `"A1:B3"`; las vacías y de texto se ignoran. */
    template <typename List> void expand(const std::string& range, List& args, Cache& cache) const {
        std::size_t colon = range.find(':');
        int c1 = 0, r1 = 0, c2 = 0, r2 = 0;
        try {
            std::tie(c1, r1) = split(range.substr(0, colon));
            std::tie(c2, r2) = split(range.substr(colon + 1));
        } catch (const std::invalid_argument&) {
            throw SheetError("#REF");
        }
        int min_col = std::min(c1, c2), max_col = std::max(c1, c2);
        int min_row = std::min(r1, r2), max_row = std::max(r1, r2);
        std::size_t cols = static_cast<std::size_t>(max_col - min_col) + 1;
        std::size_t rows = static_cast<std::size_t>(max_row - min_row) + 1;
        if (cols > m_cells.size() / rows) {
            for (const auto& [name, source] : m_cells) {
                auto [col, row] = split(name);
                if (col >= min_col && col <= max_col && row >= min_row && row <= max_row && !is_text(source))
                    args.push_back(evaluate(name, cache), 0);
            }
            return;
        }
        for (int col = min_col; col <= max_col; ++col)
            for (int row = min_row; row <= max_row; ++row) {
                std::string name = name_of(col, row);
                auto it = m_cells.find(name);
                if (it != m_cells.end() && !is_text(it->second)) args.push_back(evaluate(name, cache), 0);
            }
    }

    /** Construye el AST de la fórmula @p source (con su `=`) y lo evalúa contra esta hoja usando @p cache. */
    Number run(const std::string& source, Cache& cache) const {
        return Formula(source.substr(1))
            .evaluate([this, &cache](const std::string& name) { return evaluate(name, cache); },
                      [this, &cache](const std::string& range, auto& args) { expand(range, args, cache); });
    }

    /**
     * Valor numérico de la celda @p name (ya normalizada), memoizado en @p cache durante la consulta.
     * @throws SheetError si la celda no existe, no es numérica, tiene un ciclo o su fórmula falla.
     */
    Number evaluate(const std::string& name, Cache& cache) const {
        if (auto cached = cache.find(name); cached != cache.end()) return cached->second;
        auto it = m_cells.find(name);
        if (it == m_cells.end()) throw SheetError("#REF");
        const std::string& source = it->second;
        if (!is_formula(source)) {
            Number value;
            if (!to_number(source, value)) throw SheetError("#VALUE");
            cache.emplace(name, value);
            return value;
        }
        for (auto&& active : m_active)
            if (active.getValue() == name) throw SheetError("#CICLO");
        m_active.push_back(name, 0);
        try {
            Number value = run(source, cache);
            m_active.pop_back();
            cache.emplace(name, value);
            return value;
        } catch (...) {
            m_active.pop_back();
            throw;
        }
    }

    /** Texto que se muestra en la celda @p key (ya normalizada), sin tomar el mutex. */
    std::string display(const std::string& key, Cache& cache) const {
        auto it = m_cells.find(key);
        if (it == m_cells.end()) return "";
        if (is_text(it->second)) return it->second;
        try {
            std::ostringstream ss;
            ss << std::setprecision(10) << evaluate(key, cache);
            return ss.str();
        } catch (const SheetError& error) {
            return error.what();
        }
    }

public:
    /**
     * Guarda @p source en la celda @p name; si empieza con `=` es una fórmula, si no, un número o un texto.
     * @throws std::invalid_argument si @p name no es de la forma `A1` o @p source tiene saltos de línea.
     */
    void set(const std::string& name, const std::string& source) {
        std::string key = normalize(name);
        validate_source(source);
        std::lock_guard lock(m_mutex);
        m_cells[key] = source;
    }

    /** Borra la celda; devuelve `false` si no existía. */
    bool remove(const std::string& name) {
        std::string key = normalize(name);
        std::lock_guard lock(m_mutex);
        return m_cells.remove(key);
    }

    /**
     * Valor numérico de la celda.
     * @throws SheetError con el código del error (`#REF`, `#DIV/0`, ...).
     */
    Number value(const std::string& name) const {
        std::string key = normalize(name);
        std::lock_guard lock(m_mutex);
        Cache cache;
        return evaluate(key, cache);
    }

    /**
     * AST de la fórmula de la celda, para dibujarlo o recorrerlo; vacío si la celda no tiene fórmula.
     * @throws SheetError `#SYNTAX` si la fórmula no es válida.
     */
    Formula formula(const std::string& name) const {
        std::string key = normalize(name);
        std::lock_guard lock(m_mutex);
        auto it = m_cells.find(key);
        if (it == m_cells.end() || !is_formula(it->second)) return Formula();
        return Formula(it->second.substr(1));
    }

    /** Texto que se muestra en la celda: su número, el texto escrito, el código de error o vacío si no existe. */
    std::string text(const std::string& name) const {
        std::string key = normalize(name);
        std::lock_guard lock(m_mutex);
        Cache cache;
        return display(key, cache);
    }

    /** Dibuja la hoja como tabla con columnas de letras y filas numeradas. */
    void print(std::ostream& os) const {
        std::lock_guard lock(m_mutex);
        int cols = 0, rows = 0;
        for (const auto& [name, source] : m_cells) {
            auto [col, row] = split(name);
            cols = std::max(cols, col);
            rows = std::max(rows, row);
        }
        Cache cache;
        std::vector<std::string> grid(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));
        std::size_t width = 10;
        for (int col = 1; col <= cols; ++col) width = std::max(width, letters_of(col).size());
        for (int row = 1; row <= rows; ++row)
            for (int col = 1; col <= cols; ++col) {
                std::string cell = display(name_of(col, row), cache);
                width = std::max(width, cell.size());
                grid[static_cast<std::size_t>(row - 1) * static_cast<std::size_t>(cols) +
                     static_cast<std::size_t>(col - 1)] = std::move(cell);
            }

        int col_w = static_cast<int>(width);
        os << std::setw(5) << "" << '|';
        for (int col = 1; col <= cols; ++col) os << std::setw(col_w) << letters_of(col) << " |";
        os << '\n' << "-----+";
        std::string sep(width + 1, '-');
        for (int col = 1; col <= cols; ++col) os << sep << '+';
        os << '\n';
        for (int row = 1; row <= rows; ++row) {
            os << std::setw(4) << row << " |";
            for (int col = 1; col <= cols; ++col)
                os << std::setw(col_w)
                   << grid[static_cast<std::size_t>(row - 1) * static_cast<std::size_t>(cols) +
                           static_cast<std::size_t>(col - 1)]
                   << " |";
            os << '\n';
        }
    }

    /** Escribe las celdas como `[(celda,texto),...]` con los mixins de la HashTable; las fórmulas van entre comillas. */
    friend std::ostream& operator<<(std::ostream& os, const Sheet& sheet) {
        std::lock_guard lock(sheet.m_mutex);
        return os << sheet.m_cells;
    }

    /** Reemplaza las celdas por las leídas de @p is; si el texto, algún nombre de celda o su contenido es inválido, marca `failbit` y la hoja no cambia. */
    friend std::istream& operator>>(std::istream& is, Sheet& sheet) {
        Cells loaded, cells;
        if (!(is >> loaded)) return is;
        try {
            for (const auto& [name, source] : loaded) {
                validate_source(source);
                cells[normalize(name)] = source;
            }
        } catch (const std::invalid_argument&) {
            is.setstate(std::ios::failbit);
            return is;
        }
        std::lock_guard lock(sheet.m_mutex);
        sheet.m_cells = std::move(cells);
        return is;
    }
};

#endif // !__SPREADSHEET_H__
