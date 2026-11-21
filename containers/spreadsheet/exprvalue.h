#ifndef CONTAINERS_EXPRVALUE_H
#define CONTAINERS_EXPRVALUE_H

#include "../hashing/hashtable.h"
#include "../core/traits.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

/** Límite máximo de profundidad de anidamiento y evaluación en cascada. */
inline constexpr std::size_t expr_max_depth = 512;

/** Error al leer o evaluar una fórmula; `what()` es el código que se muestra (`#DIV/0`, `#REF`, `#SYNTAX`, `#CICLO`, `#VALUE`, `#NAME`, `#NUM`, `#LIMITE`). */
struct ExprError : std::runtime_error {
    static constexpr const char* Syntax = "#SYNTAX";
    static constexpr const char* DivZero = "#DIV/0";
    static constexpr const char* Ref = "#REF";
    static constexpr const char* Cycle = "#CICLO";
    static constexpr const char* Value = "#VALUE";
    static constexpr const char* Name = "#NAME";
    static constexpr const char* Num = "#NUM";
    static constexpr const char* Limit = "#LIMITE";
    using std::runtime_error::runtime_error;
};

/** Valor de evaluación de una fórmula: número (`N`) o cadena (`std::string`). */
template <typename N> struct ExprValue {
    static constexpr int display_precision = 10;
    std::variant<N, std::string> data;

    ExprValue() : data(N{}) {}
    ExprValue(N n) : data(n) {}
    ExprValue(std::string s) : data(std::move(s)) {}

    bool is_number() const { return std::holds_alternative<N>(data); }

    /** Devuelve el número guardado o lanza `#VALUE` si es una cadena. */
    N number() const {
        if (auto* p = std::get_if<N>(&data)) return *p;
        throw ExprError(ExprError::Value);
    }

    /** Devuelve la cadena o formatea el número como texto en locale clásico. */
    std::string str() const {
        if (auto* p = std::get_if<std::string>(&data)) return *p;
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << std::setprecision(display_precision) << std::get<N>(data);
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const ExprValue& v) { return os << v.str(); }
};

/**
 * Valor de un nodo del AST: qué es, el texto con el que se dibuja y, si es un número, su valor.
 * @tparam N Tipo numérico.
 */
template <typename N> struct ExprItem {
    /** Clase de nodo. `List` une dos argumentos de una función; `Range` es una hoja con el texto `A1:B3`. */
    enum class Kind { Number, String, Ref, Range, Negate, Binary, List, Call };

    friend std::ostream& operator<<(std::ostream& os, Kind k) { return os << static_cast<int>(k); }

    Kind kind = Kind::Number;
    std::string text;
    N number{};

    /** Escribe el texto del nodo: el operador, el número, la celda o el nombre de la función. */
    friend std::ostream& operator<<(std::ostream& os, const ExprItem& item) { return os << item.text; }
};

/** Operaciones semánticas de evaluación: conversión numérica, operadores binarios y funciones integradas. */
template <typename Number, template <typename> typename Sequence> struct ExprOps {
    using Value = ExprValue<Number>;
    using BinaryOp = Value (*)(const Value&, const Value&);
    using CallOp = Value (*)(const Sequence<Value>&);

    /**
     * Convierte @p text en número usando `std::from_chars` (independiente del locale y validado directamente sobre `Number`).
     * Sin @p used exige que se consuma todo el texto; con @p used, lee desde @p start y guarda cuántos caracteres usó.
     * @return `false` si no empieza con un número válido o desborda `Number`.
     */
    static bool to_number(const std::string& text, Number& out, std::size_t start = 0, std::size_t* used = nullptr) {
        if (start >= text.size() || std::isspace(static_cast<unsigned char>(text[start]))) return false;
        std::size_t pos = start;
        if (!used && text[pos] == '+') {
            ++pos;
            if (pos >= text.size() || (!std::isdigit(static_cast<unsigned char>(text[pos])) && text[pos] != '.'))
                return false;
        }
        Number parsed{};
        const char* first = text.data() + pos;
        const char* last = text.data() + text.size();
        auto [ptr, ec] = std::from_chars(first, last, parsed);
        if (ec != std::errc{} || ptr == first) return false;
        if constexpr (std::is_floating_point_v<Number>) {
            if (!std::isfinite(parsed)) return false;
        }
        if (!used && ptr != last) return false;
        out = parsed;
        if (used) *used = static_cast<std::size_t>(ptr - (text.data() + start));
        return true;
    }

    /** Verifica que el resultado aritmético sea finito; lanza `#NUM` en caso contrario. */
    static Number checked_number(Number val) {
        if (!std::isfinite(static_cast<long double>(val))) throw ExprError(ExprError::Num);
        return val;
    }

    /** Compara dos valores (numéricamente si ambos son números, o como texto) usando @p pred. */
    template <typename Pred> static Value compare_values(const Value& l, const Value& r, Pred pred) {
        if (l.is_number() && r.is_number()) return Value(static_cast<Number>(pred(l.number(), r.number())));
        if (!l.is_number() && !r.is_number()) return Value(static_cast<Number>(pred(l.str(), r.str())));
        throw ExprError(ExprError::Value);
    }

    /** Registro de estrategias para operadores binarios. */
    static const HashTable<HashTableTraits<std::string, BinaryOp>>& binary_ops() {
        static const HashTable<HashTableTraits<std::string, BinaryOp>> ops{
            {"+", [](const Value& l, const Value& r) -> Value { return Value(checked_number(l.number() + r.number())); }},
            {"-", [](const Value& l, const Value& r) -> Value { return Value(checked_number(l.number() - r.number())); }},
            {"*", [](const Value& l, const Value& r) -> Value { return Value(checked_number(l.number() * r.number())); }},
            {"^", [](const Value& l, const Value& r) -> Value {
                return Value(checked_number(static_cast<Number>(std::pow(l.number(), r.number()))));
            }},
            {"/", [](const Value& l, const Value& r) -> Value {
                Number den = r.number();
                if (den == 0) throw ExprError(ExprError::DivZero);
                return Value(checked_number(l.number() / den));
            }},
            {"=", [](const Value& l, const Value& r) -> Value { return compare_values(l, r, std::equal_to<>{}); }},
            {"<>", [](const Value& l, const Value& r) -> Value { return compare_values(l, r, std::not_equal_to<>{}); }},
            {"<", [](const Value& l, const Value& r) -> Value { return compare_values(l, r, std::less<>{}); }},
            {">", [](const Value& l, const Value& r) -> Value { return compare_values(l, r, std::greater<>{}); }},
            {"<=", [](const Value& l, const Value& r) -> Value { return compare_values(l, r, std::less_equal<>{}); }},
            {">=", [](const Value& l, const Value& r) -> Value { return compare_values(l, r, std::greater_equal<>{}); }},
        };
        return ops;
    }

    /** Aplica el operador binario @p op. */
    static Value apply(const std::string& op, const Value& left, const Value& right) {
        const auto& ops = binary_ops();
        auto it = ops.find(op);
        if (it == ops.end()) throw ExprError(ExprError::Syntax);
        return it->second(left, right);
    }

    /** Suma todos los valores numéricos de @p args (ignora textos). */
    static Number sum_of(const Sequence<Value>& args) {
        Number total = 0;
        for (auto&& arg : args)
            if (arg.getValue().is_number()) total += arg.getValue().number();
        return checked_number(total);
    }

    /** Cuenta cuántos valores numéricos hay en @p args. */
    static std::size_t count_numbers(const Sequence<Value>& args) {
        std::size_t count = 0;
        for (auto&& arg : args)
            if (arg.getValue().is_number()) ++count;
        return count;
    }

    /** Reduce @p args numéricos usando @p pick (ignora textos); devuelve 0 si no hay números. */
    template <typename Pick> static Number extremum_of(const Sequence<Value>& args, Pick pick) {
        bool found = false;
        Number best = 0;
        for (auto&& arg : args) {
            if (!arg.getValue().is_number()) continue;
            Number v = arg.getValue().number();
            best = found ? pick(best, v) : v;
            found = true;
        }
        return best;
    }

    /** Registro de estrategias para funciones de fórmula. */
    static const HashTable<HashTableTraits<std::string, CallOp>>& functions() {
        static const HashTable<HashTableTraits<std::string, CallOp>> table{
            {"SUMA", [](const Sequence<Value>& args) -> Value { return Value(sum_of(args)); }},
            {"CONTAR", [](const Sequence<Value>& args) -> Value { return Value(static_cast<Number>(count_numbers(args))); }},
            {"PROM", [](const Sequence<Value>& args) -> Value {
                std::size_t count = count_numbers(args);
                if (count == 0) throw ExprError(ExprError::DivZero);
                return Value(checked_number(sum_of(args) / static_cast<Number>(count)));
            }},
            {"MAX", [](const Sequence<Value>& args) -> Value {
                return Value(extremum_of(args, [](Number a, Number b) { return std::max(a, b); }));
            }},
            {"MIN", [](const Sequence<Value>& args) -> Value {
                return Value(extremum_of(args, [](Number a, Number b) { return std::min(a, b); }));
            }},
            {"CONCAT", [](const Sequence<Value>& args) -> Value {
                std::string out;
                for (auto&& arg : args) out += arg.getValue().str();
                return Value(std::move(out));
            }},
            {"LARGO", [](const Sequence<Value>& args) -> Value {
                if (args.size() != 1) throw ExprError(ExprError::Value);
                return Value(static_cast<Number>(args.at(0).getValue().str().size()));
            }},
        };
        return table;
    }

    /** Aplica la función @p name a sus argumentos. */
    static Value call(const std::string& name, const Sequence<Value>& args) {
        const auto& table = functions();
        auto it = table.find(name);
        if (it == table.end()) throw ExprError(ExprError::Name);
        return it->second(args);
    }
};

#endif // !CONTAINERS_EXPRVALUE_H
