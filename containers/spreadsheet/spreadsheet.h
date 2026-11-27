#ifndef CONTAINERS_SPREADSHEET_H
#define CONTAINERS_SPREADSHEET_H

#include "../../types.h"
#include "exprtree.h"
#include "../hashing/hashtable.h"
#include "../core/traversalops.h"
#include "../visual/visualization.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <istream>
#include <limits>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

/** Error de una celda; es el mismo error que lanza ExprTree. */
using SheetError = ExprError;

/**
 * Hoja de cálculo de texto. Las celdas se guardan en una HashTable por nombre; al pedir una celda, su fórmula se convierte en un ExprTree (AST) y se evalúa sin guardar valores calculados, reflejando cambios en cascada.
 * Soporta `+ - * / ^`, comparaciones (`= <> < > <= >=`), paréntesis, `+`/`-` unarios, referencias (`A1`), rangos (`A1:B3`) y las funciones `SUMA`, `PROM`, `MAX`, `MIN`, `CONTAR`, `SI` (con cortocircuito), `CONCAT` y `LARGO`.
 * `SUMA`/`PROM`/`MAX`/`MIN`/`CONTAR` ignoran textos (`MAX`/`MIN` devuelven `0` sin números; `PROM` lanza `#DIV/0`); un rango de más de una celda fuera de funciones agregadas produce `#VALUE`.
 * Las celdas inexistentes producen `#REF`; una celda guardada como `""` es texto vacío (`#VALUE` en aritmética), y todo texto numérico decimal finito (p. ej. `"0012"`) se interpreta como número.
 * Se guarda y se lee con `<<`/`>>` en el formato `[(celda,texto),...]` mediante `PrintableOp` y `ReadableOp`, y se dibuja con `VisualOp::visualize`. Cada operación pública toma el mutex de `LockableOp`.
 * @tparam Traits Tipos de la hoja: `number_type`, `Cells`, `Formula` y `Sequence`.
 * @tparam Ops Mixins de operaciones (`SheetTraversalOps` por defecto).
 */
template <typename Traits, typename Ops = SheetTraversalOps>
    requires SheetTraitsConcept<Traits>
class Sheet : public Ops {
public:
    using Number = typename Traits::number_type;
    using Formula = typename Traits::Formula;

private:
    using Cells = typename Traits::Cells;
    using Value = typename Formula::Value;
    using Cache = typename Traits::Cache;

    Cells m_cells;

    /** Convierte @p text a número; falla si no es un número completo. */
    static Bool to_number(const std::string& text, Number& out) { return Formula::to_number(text, out); }

    /** Indica si el texto de la celda es una fórmula. */
    static Bool is_formula(const std::string& source) { return !source.empty() && source[0] == '='; }

    /** Indica si la celda guarda texto que no es fórmula ni número. */
    static Bool is_text(const std::string& source) {
        Number ignored;
        return !is_formula(source) && !to_number(source, ignored);
    }

    /** Comprueba que el contenido de la celda no tenga saltos de línea (`\n` o `\r`). */
    static void validate_source(const std::string& source) {
        if (source.find_first_of("\r\n") != std::string::npos)
            throw std::invalid_argument("La celda no admite saltos de linea");
    }

    /** Acumula @p digit en @p acc en base @p base verificando desbordamiento de `int`. */
    static void accumulate(Int& acc, Int digit, Int base, const std::string& name) {
        constexpr Int max_int = std::numeric_limits<Int>::max();
        if (acc > (max_int - digit) / base) throw std::invalid_argument("Nombre de celda invalido: " + name);
        acc = acc * base + digit;
    }

    /** Separa `AB12` (en mayúsculas) en columna (base 26, desde 1) y fila (desde 1); lanza si es inválido o desborda `int`. */
    static std::pair<Int, Int> split(const std::string& name) {
        Int col = 0;
        std::size_t i = 0;
        for (; i < name.size() && std::isalpha(static_cast<Byte>(name[i])); ++i) {
            Byte ch = static_cast<Byte>(name[i]);
            if (!std::isupper(ch)) throw std::invalid_argument("Nombre de celda invalido: " + name);
            accumulate(col, ch - 'A' + 1, 26, name);
        }
        Int row = 0;
        for (; i < name.size(); ++i) {
            if (!std::isdigit(static_cast<Byte>(name[i])))
                throw std::invalid_argument("Nombre de celda invalido: " + name);
            accumulate(row, name[i] - '0', 10, name);
        }
        if (col < 1 || row < 1) throw std::invalid_argument("Nombre de celda invalido: " + name);
        return {col, row};
    }

    /** Pasa el nombre a mayúsculas, valida su formato y lo devuelve en forma canónica (`A01` -> `A1`). */
    static std::string normalize(std::string name) {
        std::transform(name.begin(), name.end(), name.begin(), [](Byte c) { return std::toupper(c); });
        auto [col, row] = split(name);
        return name_of(col, row);
    }

    /** Letras de la columna @p col (base 26, desde 1). */
    static std::string letters_of(Int col) {
        std::string letters;
        for (; col > 0; col = (col - 1) / 26) letters.insert(letters.begin(), static_cast<Char>('A' + (col - 1) % 26));
        return letters;
    }

    /** Nombre de la celda en la columna @p col y la fila @p row. */
    static std::string name_of(Int col, Int row) { return letters_of(col) + std::to_string(row); }

    using ActiveSet = std::unordered_set<std::string>;

    /** Normaliza @p name lanzando `SheetError::Ref` si el nombre es inválido. */
    static std::string normalize_ref(const std::string& name) {
        try {
            return normalize(name);
        } catch (const std::invalid_argument&) {
            throw SheetError(SheetError::Ref);
        }
    }

    /** Agrega el valor de @p name a @p args si no es vacío. */
    template <typename List>
    void push_cell(const std::string& name, const std::string& source, List& args, Cache& cache,
                   ActiveSet& active) const {
        if (source.empty()) return;
        args.push_back(evaluate(name, cache, active), 0);
    }

    /** Agrega a @p args las celdas no vacías del rectángulo `"A1:B3"` en orden por columna y fila. */
    template <typename List>
    void expand(const std::string& range, List& args, Cache& cache, ActiveSet& active) const {
        std::size_t colon = range.find(':');
        Int c1 = 0, r1 = 0, c2 = 0, r2 = 0;
        try {
            std::tie(c1, r1) = split(range.substr(0, colon));
            std::tie(c2, r2) = split(range.substr(colon + 1));
        } catch (const std::invalid_argument&) {
            throw SheetError(SheetError::Ref);
        }
        Int min_col = std::min(c1, c2), max_col = std::max(c1, c2);
        Int min_row = std::min(r1, r2), max_row = std::max(r1, r2);
        std::size_t cols = static_cast<std::size_t>(max_col - min_col) + 1;
        std::size_t rows = static_cast<std::size_t>(max_row - min_row) + 1;
        if (cols > m_cells.size() / rows) {
            struct Match {
                Int col;
                Int row;
                std::string name;
                const std::string* source;
            };
            std::vector<Match> matches;
            for (const auto& [name, source] : m_cells) {
                auto [col, row] = split(name);
                if (col >= min_col && col <= max_col && row >= min_row && row <= max_row)
                    matches.push_back({col, row, name, &source});
            }
            std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
                return std::tie(a.col, a.row) < std::tie(b.col, b.row);
            });
            for (const auto& match : matches) push_cell(match.name, *match.source, args, cache, active);
            return;
        }
        for (Int col = min_col;; ++col) {
            for (Int row = min_row;; ++row) {
                std::string name = name_of(col, row);
                if (auto it = m_cells.find(name); it != m_cells.end())
                    push_cell(name, it->second, args, cache, active);
                if (row == max_row) break;
            }
            if (col == max_col) break;
        }
    }

    /** Construye el AST de la fórmula @p source (con su `=`) y lo evalúa contra esta hoja usando @p cache y @p active. */
    Value run(const std::string& source, Cache& cache, ActiveSet& active) const {
        return Formula(source.substr(1))
            .evaluate([this, &cache, &active](const std::string& name) { return evaluate(name, cache, active); },
                      [this, &cache, &active](const std::string& range, auto& args) {
                          expand(range, args, cache, active);
                      });
    }

    /**
     * Valor de la celda @p name, memoizado en @p cache durante la consulta.
     * @throws SheetError si la celda no existe, tiene un ciclo o su fórmula falla.
     */
    Value evaluate(const std::string& name, Cache& cache, ActiveSet& active) const {
        std::string key = normalize_ref(name);
        if (auto cached = cache.find(key); cached != cache.end()) return cached->second;
        auto it = m_cells.find(key);
        if (it == m_cells.end()) throw SheetError(SheetError::Ref);
        const std::string& source = it->second;
        if (!is_formula(source)) {
            Number num;
            Value value = to_number(source, num) ? Value(num) : Value(source);
            cache.emplace(key, value);
            return value;
        }
        if (!active.insert(key).second) throw SheetError(SheetError::Cycle);
        if (active.size() > expr_max_depth) {
            active.erase(key);
            throw SheetError(SheetError::Limit);
        }
        try {
            Value value = run(source, cache, active);
            active.erase(key);
            cache.emplace(key, value);
            return value;
        } catch (...) {
            active.erase(key);
            throw;
        }
    }

    /** Texto que se muestra en la celda @p key (ya normalizada), sin tomar el mutex. */
    std::string display(const std::string& key, Cache& cache, ActiveSet& active) const {
        auto it = m_cells.find(key);
        if (it == m_cells.end()) return "";
        if (is_text(it->second)) return it->second;
        try {
            return evaluate(key, cache, active).str();
        } catch (const SheetError& error) {
            return error.what();
        }
    }

    /** Copia o mueve las celdas desde @p other bloqueando ambos mutexes. */
    template <typename Other> void assign_from(Other&& other) {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        m_cells = std::forward<Other>(other).m_cells;
    }

public:
    Sheet() = default;

    /** Copia la hoja bloqueando el mutex de @p other. */
    Sheet(const Sheet& other) : Ops(other) {
        std::lock_guard lock(other.m_mutex);
        m_cells = other.m_cells;
    }

    /** Mueve la hoja bloqueando el mutex de @p other. */
    Sheet(Sheet&& other) : Ops(std::move(other)) {
        std::lock_guard lock(other.m_mutex);
        m_cells = std::move(other.m_cells);
    }

    /** Asigna por copia bloqueando ambos mutexes. */
    Sheet& operator=(const Sheet& other) {
        assign_from(other);
        return *this;
    }

    /** Asigna por movimiento bloqueando ambos mutexes. */
    Sheet& operator=(Sheet&& other) {
        assign_from(std::move(other));
        return *this;
    }

    /**
     * Guarda @p source en la celda @p name; si empieza con `=` es una fórmula, si no, un número o un texto.
     * @throws std::invalid_argument si @p name no es de la forma `A1` o @p source tiene saltos de línea.
     */
    void set(const std::string& name, const std::string& source) {
        std::string key = normalize(name);
        validate_source(source);
        std::lock_guard lock(this->m_mutex);
        m_cells[key] = source;
    }

    /** Borra la celda; devuelve `false` si no existía. */
    Bool remove(const std::string& name) {
        std::string key = normalize(name);
        std::lock_guard lock(this->m_mutex);
        return m_cells.remove(key);
    }

    /**
     * Valor numérico de la celda.
     * @throws SheetError con el código del error (`#REF`, `#DIV/0`, ...).
     */
    Number value(const std::string& name) const {
        std::string key = normalize_ref(name);
        std::lock_guard lock(this->m_mutex);
        Cache cache;
        ActiveSet active;
        return evaluate(key, cache, active).number();
    }

    /**
     * AST de la fórmula de la celda, para dibujarlo o recorrerlo; vacío si la celda no existe o no tiene fórmula.
     * @throws SheetError `#REF` si el nombre es inválido o `#SYNTAX` si la fórmula no es válida.
     */
    Formula formula(const std::string& name) const {
        std::string key = normalize_ref(name);
        std::lock_guard lock(this->m_mutex);
        auto it = m_cells.find(key);
        if (it == m_cells.end() || !is_formula(it->second)) return Formula();
        return Formula(it->second.substr(1));
    }

    /** Texto que se muestra en la celda: su número, el texto escrito, el código de error o vacío si no existe. */
    std::string text(const std::string& name) const {
        std::string key;
        try {
            key = normalize_ref(name);
        } catch (const SheetError& error) {
            return error.what();
        }
        std::lock_guard lock(this->m_mutex);
        Cache cache;
        ActiveSet active;
        return display(key, cache, active);
    }

    /** Vista de tabla autocontenida (snapshot disperso) para `visual::print_table`. */
    struct TableView {
        struct Entry {
            Int row;
            Int col;
            std::string text;
        };

        std::vector<Entry> m_entries;
        Int m_cols = 0;
        Int m_rows = 0;

        Int rows() const { return m_rows; }
        Int cols() const { return m_cols; }
        std::string col_header(Int col) const { return letters_of(col); }
        std::string cell_text(Int row, Int col) const {
            auto it = std::lower_bound(m_entries.begin(), m_entries.end(), std::pair{row, col},
                                       [](const Entry& e, const std::pair<Int, Int>& key) {
                                           return std::tie(e.row, e.col) < key;
                                       });
            if (it != m_entries.end() && it->row == row && it->col == col) return it->text;
            return "";
        }
    };

    /** Vista de tabla autocontenida tomada bajo `m_mutex` para `visual::print_table`. */
    TableView table_view() const {
        std::lock_guard lock(this->m_mutex);
        TableView view;
        Cache cache;
        ActiveSet active;
        for (const auto& [name, source] : m_cells) {
            std::string rendered = display(name, cache, active);
            if (!rendered.empty()) {
                auto [col, row] = split(name);
                view.m_cols = std::max(view.m_cols, col);
                view.m_rows = std::max(view.m_rows, row);
                view.m_entries.push_back({row, col, std::move(rendered)});
            }
        }
        std::sort(view.m_entries.begin(), view.m_entries.end(),
                  [](const typename TableView::Entry& a, const typename TableView::Entry& b) {
                      return std::tie(a.row, a.col) < std::tie(b.row, b.col);
                  });
        return view;
    }

    /**
     * Dibuja la hoja como tabla con columnas de letras y filas numeradas.
     * @throws std::length_error si las dimensiones superan `visual::max_table_cells` (`100'000` celdas).
     */
    void print_table(std::ostream& os) const {
        TableView view = table_view();
        visual::check_table_bounds(view.cols(), view.rows());
        visual::print_table(os, view);
    }

    /**
     * Dibuja la hoja como tabla con columnas de letras y filas numeradas.
     * @throws std::length_error si las dimensiones superan `visual::max_table_cells` (`100'000` celdas).
     */
    void print(std::ostream& os) const { print_table(os); }

    /** Escribe las celdas como `[(celda,texto),...]` con los mixins de la HashTable; usado por `PrintableOp`. */
    std::ostream& write_stream(std::ostream& os) const {
        std::lock_guard lock(this->m_mutex);
        return os << m_cells;
    }

    /** Reemplaza las celdas por las leídas de @p is; si el texto, algún nombre, salto de línea o duplicado canónico es inválido, marca `failbit` y la hoja no cambia (las fórmulas se evalúan de forma perezosa). Usado por `ReadableOp`. */
    std::istream& read_stream(std::istream& is) {
        Cells loaded, cells;
        if (!(is >> loaded)) return is;
        try {
            for (const auto& [name, source] : loaded) {
                validate_source(source);
                std::string key = normalize(name);
                if (cells.contains(key)) {
                    is.setstate(std::ios::failbit);
                    return is;
                }
                cells[key] = source;
            }
        } catch (const std::invalid_argument&) {
            is.setstate(std::ios::failbit);
            return is;
        }
        std::lock_guard lock(this->m_mutex);
        m_cells = std::move(cells);
        return is;
    }
};

#endif // !CONTAINERS_SPREADSHEET_H
