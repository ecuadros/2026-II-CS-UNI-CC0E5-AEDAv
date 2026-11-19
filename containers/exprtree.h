#ifndef __EXPRTREE_H__
#define __EXPRTREE_H__

#include "binarytree.h"
#include "traits.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

/** Error al leer o evaluar una fórmula; `what()` es el código que se muestra (`#DIV/0`, `#REF`, `#SYNTAX`, `#CICLO`, `#VALUE`, `#NAME`). */
struct ExprError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * Valor de un nodo del AST: qué es, el texto con el que se dibuja y, si es un número, su valor.
 * @tparam N Tipo numérico.
 */
template <typename N> struct ExprItem {
    /** Clase de nodo. `List` une dos argumentos de una función; `Range` es una hoja con el texto `A1:B3`. */
    enum class Kind { Number, Ref, Range, Negate, Binary, List, Call };

    Kind kind = Kind::Number;
    std::string text;
    N number{};

    /** Escribe el texto del nodo: el operador, el número, la celda o el nombre de la función. */
    friend std::ostream& operator<<(std::ostream& os, const ExprItem& item) { return os << item.text; }
};

/**
 * Árbol de sintaxis abstracta de una fórmula. El constructor la lee con un parser de descenso recursivo y arma los nodos a mano, como MerkleTree; evaluate() la recorre en postorden con una pila.
 * Los paréntesis no generan nodos: solo deciden qué queda debajo de qué. Los argumentos de una función se encadenan con nodos `,` hacia la derecha.
 * @tparam Traits Tipos de número, valor, nodo, secuencia e iteradores.
 * @tparam Ops Mixins de recorrido, impresión y dibujo.
 */
template <typename Traits, typename Ops> class ExprTree : public BinaryTreeBase<Traits, Ops> {
public:
    using Base = BinaryTreeBase<Traits, Ops>;
    using Number = typename Traits::number_type;
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using Kind = typename value_type::Kind;
    template <typename T> using Sequence = typename Traits::template Sequence<T>;

private:
    std::size_t m_size = 0;

    /** Libera un subárbol que todavía no cuelga del árbol. */
    struct Free {
        void operator()(NodePtr node) const { destroy(node); }
    };
    using Owned = std::unique_ptr<Node, Free>;

    enum class Tok { Number, Ref, Ident, Op, LParen, RParen, Comma, Colon, End };

    /** Unidad léxica de una fórmula. */
    struct Token {
        Tok type = Tok::End;
        std::string text;
        Number number{};
    };

    /** Mayúsculas de un identificador o referencia. */
    static std::string upper(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::toupper(c); });
        return text;
    }

    /** Convierte el texto de una fórmula en tokens; el último siempre es `End`. */
    static Sequence<Token> tokenize(const std::string& s) {
        Sequence<Token> tokens;
        std::size_t i = 0;
        while (i < s.size()) {
            unsigned char c = s[i];
            Token token;
            if (std::isspace(c)) {
                ++i;
                continue;
            }
            if (std::isdigit(c) || c == '.') {
                std::size_t used = 0;
                if (!to_number(s, token.number, i, &used)) throw ExprError("#SYNTAX");
                token.type = Tok::Number;
                token.text = s.substr(i, used);
                i += used;
            } else if (std::isalpha(c)) {
                std::size_t j = i;
                while (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) ++j;
                std::size_t k = j;
                while (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) ++k;
                token.type = k > j ? Tok::Ref : Tok::Ident;
                token.text = upper(s.substr(i, k - i));
                i = k;
            } else {
                token.text = std::string(1, s[i++]);
                switch (token.text[0]) {
                case '+': case '-': case '*': case '/': case '^': token.type = Tok::Op; break;
                case '(': token.type = Tok::LParen; break;
                case ')': token.type = Tok::RParen; break;
                case ',': token.type = Tok::Comma; break;
                case ':': token.type = Tok::Colon; break;
                default: throw ExprError("#SYNTAX");
                }
            }
            tokens.push_back(token, 0);
        }
        tokens.push_back(Token{}, 0);
        return tokens;
    }

    /**
     * Parser de descenso recursivo que construye el AST.
     * Gramática: expr := term (('+'|'-') term)*, term := power (('*'|'/') power)*, power := unary ('^' power)?,
     * unary := ('+'|'-') unary | primary, primary := número | celda | '(' expr ')' | función '(' argumentos ')'.
     */
    class Parser {
        ExprTree& m_tree;
        Sequence<Token> m_tokens;
        std::size_t m_pos = 0;

        /** Token @p ahead posiciones adelante, sin consumirlo. */
        const Token& peek(std::size_t ahead = 0) const {
            return m_tokens.at(std::min(m_pos + ahead, m_tokens.size() - 1)).getValue();
        }
        /** Consume y devuelve el token actual. */
        Token take() {
            Token token = peek();
            if (token.type != Tok::End) ++m_pos;
            return token;
        }
        /** Consume el token si es del tipo @p type. */
        bool accept(Tok type) {
            if (peek().type != type) return false;
            ++m_pos;
            return true;
        }
        /** Consume un token de tipo @p type o falla. */
        void expect(Tok type) {
            if (!accept(type)) throw ExprError("#SYNTAX");
        }
        /** Indica si el token actual es el operador @p op. */
        bool is_op(char op) const { return peek().type == Tok::Op && peek().text[0] == op; }

        /** Crea un nodo con hijos @p left y @p right; si `new` falla, los hijos se liberan solos. */
        Owned make(value_type item, Owned left = nullptr, Owned right = nullptr) {
            Owned node(new Node(std::move(item), Ref(-1)));
            ++m_tree.m_size;
            for (bool side : {false, true}) {
                Owned& child = side ? right : left;
                if (child) child->m_pParent = node.get();
                node->m_pChild[side] = child.release();
            }
            return node;
        }

        /** Nodo binario con el operador @p op. */
        Owned binary(char op, Owned left, Owned right) {
            return make({Kind::Binary, std::string(1, op)}, std::move(left), std::move(right));
        }

        Owned expr() {
            Owned left = term();
            while (is_op('+') || is_op('-')) {
                char op = take().text[0];
                left = binary(op, std::move(left), term());
            }
            return left;
        }

        Owned term() {
            Owned left = power();
            while (is_op('*') || is_op('/')) {
                char op = take().text[0];
                left = binary(op, std::move(left), power());
            }
            return left;
        }

        Owned power() {
            Owned base = unary();
            if (!is_op('^')) return base;
            take();
            return binary('^', std::move(base), power());
        }

        Owned unary() {
            if (is_op('+')) return take(), unary();
            if (!is_op('-')) return primary();
            take();
            return make({Kind::Negate, "-"}, unary());
        }

        Owned primary() {
            Token token = take();
            switch (token.type) {
            case Tok::Number: return make({Kind::Number, token.text, token.number});
            case Tok::Ref: return make({Kind::Ref, token.text});
            case Tok::LParen: {
                Owned inner = expr();
                expect(Tok::RParen);
                return inner;
            }
            case Tok::Ident: {
                expect(Tok::LParen);
                Owned args = peek().type == Tok::RParen ? nullptr : arguments();
                expect(Tok::RParen);
                return make({Kind::Call, token.text}, std::move(args));
            }
            default: throw ExprError("#SYNTAX");
            }
        }

        /** Un argumento: un rango `A1:B3` (hoja `Range`) o una expresión. */
        Owned argument() {
            if (peek().type != Tok::Ref || peek(1).type != Tok::Colon) return expr();
            std::string from = take().text;
            take();
            if (peek().type != Tok::Ref) throw ExprError("#SYNTAX");
            return make({Kind::Range, from + ":" + take().text});
        }

        /** Argumentos separados por `,`, encadenados hacia la derecha: `a, b, c` es `,(a, ,(b, c))`. */
        Owned arguments() {
            Owned first = argument();
            if (!accept(Tok::Comma)) return first;
            return make({Kind::List, ","}, std::move(first), arguments());
        }

    public:
        /** Prepara el parser de @p formula para el árbol @p tree. */
        Parser(ExprTree& tree, const std::string& formula) : m_tree(tree), m_tokens(tokenize(formula)) {}

        /** Lee toda la fórmula y devuelve la raíz del AST. */
        Owned run() {
            Owned root = expr();
            if (peek().type != Tok::End) throw ExprError("#SYNTAX");
            return root;
        }
    };

    /** Aplica el operador binario @p op. */
    static Number apply(char op, Number left, Number right) {
        switch (op) {
        case '+': return left + right;
        case '-': return left - right;
        case '*': return left * right;
        case '^': return std::pow(left, right);
        default:
            if (right == 0) throw ExprError("#DIV/0");
            return left / right;
        }
    }

    /** Aplica la función @p name a sus argumentos. */
    static Number call(const std::string& name, const Sequence<Number>& args) {
        Number sum = 0;
        for (auto&& arg : args) sum += arg.getValue();
        if (name == "SUMA") return sum;
        if (name == "CONTAR") return static_cast<Number>(args.size());
        if (name == "PROM") {
            if (args.empty()) throw ExprError("#DIV/0");
            return sum / static_cast<Number>(args.size());
        }
        if (name == "MAX" || name == "MIN") {
            if (args.empty()) return 0;
            Number best = args.at(0).getValue();
            for (auto&& arg : args)
                best = name == "MAX" ? std::max(best, arg.getValue()) : std::min(best, arg.getValue());
            return best;
        }
        throw ExprError("#NAME");
    }

protected:
    /** Libera los nodos y pone el tamaño a cero. */
    void reset() override {
        Base::reset();
        m_size = 0;
    }

    /** Dibuja solo el texto del nodo (`+`, `A1`, `SUMA`), sin su referencia. */
    void print_node(std::ostream& os, const Node& node) const override { os << node.getValue(); }

public:
    /**
     * Convierte @p text en número. Sin @p used exige que se consuma todo el texto; con @p used, lee desde @p start y guarda cuántos caracteres usó.
     * @return `false` si no empieza con un número.
     */
    static bool to_number(const std::string& text, Number& out, std::size_t start = 0, std::size_t* used = nullptr) {
        try {
            std::size_t n = 0;
            out = static_cast<Number>(std::stold(text.substr(start), &n));
            if (used) *used = n;
            return used || start + n == text.size();
        } catch (const std::logic_error&) {
            return false;
        }
    }

    /** Crea un árbol vacío. */
    ExprTree() = default;

    /**
     * Construye el AST de @p formula (sin el `=` inicial).
     * @throws ExprError `#SYNTAX` si la fórmula no es válida.
     */
    explicit ExprTree(const std::string& formula) {
        this->m_root = Parser(*this, formula).run().release();
    }

    /** Copia en profundidad @p other. */
    ExprTree(const ExprTree& other) : Base(other) {
        std::lock_guard lock(other.m_mutex);
        m_size = other.m_size;
    }

    /** Mueve el contenido de @p other, que queda vacío. */
    ExprTree(ExprTree&& other) noexcept { swap(other); }

    /** Asigna por copia o por movimiento (copy-and-swap). */
    ExprTree& operator=(ExprTree other) noexcept {
        swap(other);
        return *this;
    }

    /** Intercambia el contenido con @p other. */
    void swap(ExprTree& other) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        std::swap(this->m_root, other.m_root);
        std::swap(m_size, other.m_size);
    }

    /** Cantidad de nodos. */
    std::size_t size() const override {
        std::lock_guard lock(this->m_mutex);
        return m_size;
    }

    /**
     * Evalúa el AST recorriéndolo en postorden: cada hoja apila sus valores y cada operador desapila los de sus hijos.
     * La pila guarda listas de números porque un rango aporta varios; un operador exige listas de un solo valor.
     * @param resolve Devuelve el valor de una celda: `Number(const std::string&)`.
     * @param expand Agrega a la lista los valores de un rango `"A1:B3"`: `void(const std::string&, Sequence<Number>&)`.
     * @throws ExprError con el código del error.
     */
    template <typename Resolve, typename Expand> Number evaluate(Resolve&& resolve, Expand&& expand) const {
        std::lock_guard lock(this->m_mutex);
        if (!this->m_root) throw ExprError("#SYNTAX");
        Sequence<Sequence<Number>> stack;
        auto push = [&](Number value) {
            Sequence<Number> list;
            list.push_back(value, 0);
            stack.push_back(list, 0);
        };
        auto pop = [&] {
            Sequence<Number> top = stack.at(stack.size() - 1).getValue();
            stack.pop_back();
            return top;
        };
        auto single = [&] {
            Sequence<Number> list = pop();
            if (list.size() != 1) throw ExprError("#VALUE");
            return list.at(0).getValue();
        };
        for (const auto& node : this->PostOrder()) {
            const value_type& item = node.getValue();
            switch (item.kind) {
            case Kind::Number: push(item.number); break;
            case Kind::Ref: push(resolve(item.text)); break;
            case Kind::Range: {
                Sequence<Number> list;
                expand(item.text, list);
                stack.push_back(list, 0);
                break;
            }
            case Kind::Negate: push(-single()); break;
            case Kind::Binary: {
                Number right = single();
                Number left = single();
                push(apply(item.text[0], left, right));
                break;
            }
            case Kind::List: {
                Sequence<Number> right = pop();
                Sequence<Number> left = pop();
                for (auto&& value : right) left.push_back(value.getValue(), 0);
                stack.push_back(left, 0);
                break;
            }
            case Kind::Call: {
                Sequence<Number> args;
                if (node.m_pChild[0]) args = pop();
                push(call(item.text, args));
                break;
            }
            }
        }
        return single();
    }
};

#endif // !__EXPRTREE_H__
