#ifndef CONTAINERS_EXPRTREE_H
#define CONTAINERS_EXPRTREE_H

#include "binarytree.h"
#include "exprvalue.h"
#include "hashtable.h"
#include "traits.h"
#include "vector.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

/**
 * Árbol de sintaxis abstracta de una fórmula. El constructor la lee con un parser de descenso recursivo y arma los nodos a mano, como MerkleTree; `evaluate()` la evalúa de forma iterativa con pila explícita y cortocircuito en `SI`.
 * Los paréntesis no generan nodos: solo deciden qué queda debajo de qué. Los argumentos de una función se encadenan con nodos `,` hacia la derecha.
 * @tparam Traits Tipos de número, valor, nodo, secuencia e iteradores.
 * @tparam Ops Mixins de recorrido, impresión y dibujo.
 */
template <typename Traits, typename Ops> class ExprTree : public BinaryTreeBase<Traits, Ops> {
public:
    using Base = BinaryTreeBase<Traits, Ops>;
    using Number = typename Traits::number_type;
    using Value = ExprValue<Number>;
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using Kind = typename value_type::Kind;
    template <typename T> using Sequence = typename Traits::template Sequence<T>;

private:
    static constexpr std::string_view IfFunction = "SI";
    using EvalOps = ExprOps<Number, Sequence>;
    std::size_t m_size = 0;

    /** Libera un subárbol de forma iterativa. */
    static void destroy_subtree(NodePtr root) {
        if (!root) return;
        Sequence<NodePtr> stack;
        stack.push_back(root, 0);
        while (!stack.empty()) {
            NodePtr cur = stack.at(stack.size() - 1).getValue();
            stack.pop_back();
            if (cur->m_pChild[0]) stack.push_back(cur->m_pChild[0], 0);
            if (cur->m_pChild[1]) stack.push_back(cur->m_pChild[1], 0);
            delete cur;
        }
    }

    /** Libera un subárbol que todavía no cuelga del árbol. */
    struct Free {
        void operator()(NodePtr node) const { destroy_subtree(node); }
    };
    using Owned = std::unique_ptr<Node, Free>;

    enum class Tok { Number, String, Ref, Ident, Op, LParen, RParen, Comma, Colon, End };
    friend std::ostream& operator<<(std::ostream& os, Tok t) { return os << static_cast<int>(t); }

    enum class TaskOp { Eval, Negate, Binary, SiBranch, ExpandRange, Call };
    friend std::ostream& operator<<(std::ostream& os, TaskOp t) { return os << static_cast<int>(t); }

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

    /** Tabla de signos de puntuación reconocidos por el lexer. */
    static const HashTable<HashTableTraits<char, Tok>>& punctuation() {
        static const HashTable<HashTableTraits<char, Tok>> table{
            {'(', Tok::LParen},
            {')', Tok::RParen},
            {',', Tok::Comma},
            {':', Tok::Colon},
        };
        return table;
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
            if (c == '"') {
                std::string text;
                std::size_t j = i + 1;
                bool closed = false;
                while (j < s.size()) {
                    if (s[j] == '\\' && j + 1 < s.size() && (s[j + 1] == '"' || s[j + 1] == '\\')) {
                        text.push_back(s[j + 1]);
                        j += 2;
                    } else if (s[j] == '"') {
                        closed = true;
                        ++j;
                        break;
                    } else {
                        text.push_back(s[j++]);
                    }
                }
                if (!closed) throw ExprError(ExprError::Syntax);
                token.type = Tok::String;
                token.text = std::move(text);
                i = j;
            } else if (std::isdigit(c) || c == '.') {
                std::size_t used = 0;
                if (!to_number(s, token.number, i, &used)) throw ExprError(ExprError::Syntax);
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
                if (i + 1 < s.size() && EvalOps::binary_ops().contains(s.substr(i, 2))) {
                    token.type = Tok::Op;
                    token.text = s.substr(i, 2);
                    i += 2;
                } else {
                    char sym = s[i++];
                    token.text = std::string(1, sym);
                    if (EvalOps::binary_ops().contains(token.text)) {
                        token.type = Tok::Op;
                    } else if (auto it = punctuation().find(sym); it != punctuation().end()) {
                        token.type = it->second;
                    } else {
                        throw ExprError(ExprError::Syntax);
                    }
                }
            }
            tokens.push_back(token, 0);
        }
        tokens.push_back(Token{}, 0);
        return tokens;
    }

    /**
     * Parser de descenso recursivo que construye el AST.
     * Gramática: compare := expr (('='|'<>'|'<'|'>'|'<='|'>=') expr)*, expr := term (('+'|'-') term)*,
     * term := power (('*'|'/') power)*, power := unary ('^' power)?, unary := ('+'|'-') unary | primary,
     * primary := número | cadena | celda | '(' compare ')' | función '(' argumentos ')'.
     */
    class Parser {
        static constexpr int max_depth = static_cast<int>(expr_max_depth);
        ExprTree& m_tree;
        Sequence<Token> m_tokens;
        std::size_t m_pos = 0;
        int m_depth = 0;

        struct DepthGuard {
            int& depth;
            explicit DepthGuard(int& d) : depth(d) {
                if (++depth > max_depth) throw ExprError(ExprError::Syntax);
            }
            ~DepthGuard() { --depth; }
        };

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
            if (!accept(type)) throw ExprError(ExprError::Syntax);
        }
        /** Indica si el token actual es el operador @p op. */
        bool is_op(std::string_view op) const { return peek().type == Tok::Op && peek().text == op; }

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
        Owned binary(std::string op, Owned left, Owned right) {
            return make({Kind::Binary, std::move(op)}, std::move(left), std::move(right));
        }

        /** Analiza una secuencia asociativa a la izquierda de operadores @p ops sobre el nivel @p next. */
        template <typename Next> Owned left_assoc(Next&& next, std::initializer_list<const char*> ops) {
            Owned left = next();
            auto matches = [&] {
                return std::any_of(ops.begin(), ops.end(), [&](const char* op) { return is_op(op); });
            };
            while (matches()) {
                std::string op = take().text;
                left = binary(std::move(op), std::move(left), next());
            }
            return left;
        }

        Owned compare() {
            return left_assoc([this] { return expr(); }, {"=", "<", ">", "<=", ">=", "<>"});
        }

        Owned expr() {
            return left_assoc([this] { return term(); }, {"+", "-"});
        }

        Owned term() {
            return left_assoc([this] { return power(); }, {"*", "/"});
        }

        Owned power() {
            Owned base = unary();
            if (!is_op("^")) return base;
            DepthGuard guard(m_depth);
            take();
            return binary("^", std::move(base), power());
        }

        Owned unary() {
            if (is_op("+")) {
                DepthGuard guard(m_depth);
                take();
                return unary();
            }
            if (!is_op("-")) return primary();
            DepthGuard guard(m_depth);
            take();
            return make({Kind::Negate, "-"}, unary());
        }

        Owned primary() {
            DepthGuard guard(m_depth);
            Token token = take();
            switch (token.type) {
            case Tok::Number: return make({Kind::Number, token.text, token.number});
            case Tok::String: return make({Kind::String, token.text});
            case Tok::Ref: return make({Kind::Ref, token.text});
            case Tok::LParen: {
                Owned inner = compare();
                expect(Tok::RParen);
                return inner;
            }
            case Tok::Ident: {
                expect(Tok::LParen);
                Owned args = peek().type == Tok::RParen ? nullptr : arguments();
                expect(Tok::RParen);
                return make({Kind::Call, token.text}, std::move(args));
            }
            default: throw ExprError(ExprError::Syntax);
            }
        }

        /** Un argumento: un rango `A1:B3` (hoja `Range`) o una expresión. */
        Owned argument() {
            if (peek().type != Tok::Ref || peek(1).type != Tok::Colon) return compare();
            std::string from = take().text;
            take();
            if (peek().type != Tok::Ref) throw ExprError(ExprError::Syntax);
            return make({Kind::Range, from + ":" + take().text});
        }

        /** Argumentos separados por `,`, encadenados hacia la derecha: `a, b, c` es `,(a, ,(b, c))`. */
        Owned arguments() {
            DepthGuard guard(m_depth);
            Owned first = argument();
            if (!accept(Tok::Comma)) return first;
            return make({Kind::List, ","}, std::move(first), arguments());
        }

    public:
        /** Prepara el parser de @p formula para el árbol @p tree. */
        Parser(ExprTree& tree, const std::string& formula) : m_tree(tree), m_tokens(tokenize(formula)) {}

        /** Lee toda la fórmula y devuelve la raíz del AST. */
        Owned run() {
            Owned root = compare();
            if (peek().type != Tok::End) throw ExprError(ExprError::Syntax);
            return root;
        }
    };

protected:
    /** Libera los nodos de forma iterativa y pone el tamaño a cero. */
    void reset() override {
        destroy_subtree(std::exchange(this->m_root, nullptr));
        m_size = 0;
    }

    /** Dibuja solo el texto del nodo (`+`, `A1`, `SUMA`), sin su referencia. */
    void print_node(std::ostream& os, const Node& node) const override { os << node.getValue(); }

public:
    /**
     * Convierte @p text en número usando `ExprOps::to_number`.
     * @return `false` si no empieza con un número válido o desborda `Number`.
     */
    static bool to_number(const std::string& text, Number& out, std::size_t start = 0, std::size_t* used = nullptr) {
        return EvalOps::to_number(text, out, start, used);
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

    /** Clona de forma iterativa el subárbol @p src con seguridad ante excepciones. */
    static Owned clone_subtree(const Node* src) {
        if (!src) return nullptr;
        Owned root(new Node(src->getValue(), src->getRef()));
        struct Frame {
            const Node* src = nullptr;
            NodePtr dst = nullptr;
        };
        Sequence<Frame> stack;
        stack.push_back({src, root.get()}, 0);
        while (!stack.empty()) {
            auto [s, d] = stack.at(stack.size() - 1).getValue();
            stack.pop_back();
            for (bool side : {false, true}) {
                if (const Node* child = s->m_pChild[side]) {
                    Owned copy(new Node(child->getValue(), child->getRef()));
                    copy->m_pParent = d;
                    d->m_pChild[side] = copy.release();
                    stack.push_back({child, d->m_pChild[side]}, 0);
                }
            }
        }
        return root;
    }

    /** Destructor iterativo para evitar desbordamiento de pila en ASTs profundos. */
    ~ExprTree() override {
        destroy_subtree(std::exchange(this->m_root, nullptr));
        m_size = 0;
    }

    /** Copia en profundidad @p other bajo un único bloqueo de `other.m_mutex`. */
    ExprTree(const ExprTree& other) : Base() {
        std::lock_guard lock(other.m_mutex);
        this->m_root = clone_subtree(other.m_root).release();
        m_size = other.m_size;
    }

    /** Mueve el contenido de @p other, que queda vacío. */
    ExprTree(ExprTree&& other) { swap(other); }

    /** Asigna por copia o por movimiento (copy-and-swap). */
    ExprTree& operator=(ExprTree other) {
        swap(other);
        return *this;
    }

    /** Intercambia el contenido con @p other. */
    void swap(ExprTree& other) {
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
     * Evalúa el AST de forma iterativa con pila explícita; `SI(cond, a, b)` evalúa en cortocircuito solo la rama elegida.
     * Un rango de más de una celda fuera de funciones agregadas (p. ej. en `SI` o `LARGO`) produce `#VALUE`.
     * @param resolve Devuelve el valor de una celda: `Value(const std::string&)`.
     * @param expand Agrega a la lista los valores de un rango `"A1:B3"`: `void(const std::string&, Sequence<Value>&)`.
     * @throws ExprError con el código del error.
     */
    template <typename Resolve, typename Expand> Value evaluate(Resolve&& resolve, Expand&& expand) const {
        std::lock_guard lock(this->m_mutex);
        if (!this->m_root) throw ExprError(ExprError::Syntax);

        struct Task {
            using Op = TaskOp;
            Op op = Op::Eval;
            NodePtr node = nullptr;
            std::size_t base = 0;
        };

        struct EvalCtx {
            Resolve& resolve;
            Expand& expand;
            Sequence<Task> tasks;
            Sequence<Value> values;

            static void collect_arg_nodes(NodePtr arg_root, Sequence<NodePtr>& out) {
                NodePtr cur = arg_root;
                while (cur && cur->getValue().kind == Kind::List) {
                    if (cur->m_pChild[0]) out.push_back(cur->m_pChild[0], 0);
                    cur = cur->m_pChild[1];
                }
                if (cur) out.push_back(cur, 0);
            }
        };

        using KindHandler = void (*)(EvalCtx&, NodePtr);
        using TaskHandler = void (*)(EvalCtx&, const Task&);

        static const HashTable<HashTableTraits<Kind, KindHandler>> kind_handlers{
            {Kind::Number, [](EvalCtx& ctx, NodePtr node) { ctx.values.push_back(Value(node->getValue().number), 0); }},
            {Kind::String, [](EvalCtx& ctx, NodePtr node) { ctx.values.push_back(Value(node->getValue().text), 0); }},
            {Kind::Ref, [](EvalCtx& ctx, NodePtr node) { ctx.values.push_back(ctx.resolve(node->getValue().text), 0); }},
            {Kind::Range, [](EvalCtx& ctx, NodePtr node) {
                Sequence<Value> list;
                ctx.expand(node->getValue().text, list);
                if (list.size() != 1) throw ExprError(ExprError::Value);
                ctx.values.push_back(list.at(0).getValue(), 0);
            }},
            {Kind::Negate, [](EvalCtx& ctx, NodePtr node) {
                ctx.tasks.push_back({Task::Op::Negate, node, 0}, 0);
                ctx.tasks.push_back({Task::Op::Eval, node->m_pChild[0], 0}, 0);
            }},
            {Kind::Binary, [](EvalCtx& ctx, NodePtr node) {
                ctx.tasks.push_back({Task::Op::Binary, node, 0}, 0);
                ctx.tasks.push_back({Task::Op::Eval, node->m_pChild[1], 0}, 0);
                ctx.tasks.push_back({Task::Op::Eval, node->m_pChild[0], 0}, 0);
            }},
            {Kind::List, [](EvalCtx&, NodePtr) { throw ExprError(ExprError::Syntax); }},
            {Kind::Call, [](EvalCtx& ctx, NodePtr node) {
                Sequence<NodePtr> arg_nodes;
                if (node->m_pChild[0]) EvalCtx::collect_arg_nodes(node->m_pChild[0], arg_nodes);
                if (node->getValue().text == IfFunction) {
                    if (arg_nodes.size() != 3) throw ExprError(ExprError::Value);
                    ctx.tasks.push_back({Task::Op::SiBranch, node, 0}, 0);
                    ctx.tasks.push_back({Task::Op::Eval, arg_nodes.at(0).getValue(), 0}, 0);
                    return;
                }
                ctx.tasks.push_back({Task::Op::Call, node, ctx.values.size()}, 0);
                for (std::size_t i = arg_nodes.size(); i > 0; --i) {
                    NodePtr arg = arg_nodes.at(i - 1).getValue();
                    auto op = arg->getValue().kind == Kind::Range ? Task::Op::ExpandRange : Task::Op::Eval;
                    ctx.tasks.push_back({op, arg, 0}, 0);
                }
            }},
        };

        static const HashTable<HashTableTraits<TaskOp, TaskHandler>> task_handlers{
            {Task::Op::Eval, [](EvalCtx& ctx, const Task& task) { kind_handlers.at(task.node->getValue().kind)(ctx, task.node); }},
            {Task::Op::Negate, [](EvalCtx& ctx, const Task&) {
                Value& top = ctx.values.at(ctx.values.size() - 1).value();
                top = Value(EvalOps::checked_number(-top.number()));
            }},
            {Task::Op::Binary, [](EvalCtx& ctx, const Task& task) {
                Value right = std::move(ctx.values.at(ctx.values.size() - 1).value());
                ctx.values.pop_back();
                Value left = std::move(ctx.values.at(ctx.values.size() - 1).value());
                ctx.values.at(ctx.values.size() - 1).value() = EvalOps::apply(task.node->getValue().text, left, right);
            }},
            {Task::Op::SiBranch, [](EvalCtx& ctx, const Task& task) {
                bool cond = ctx.values.at(ctx.values.size() - 1).getValue().number() != 0;
                ctx.values.pop_back();
                Sequence<NodePtr> arg_nodes;
                EvalCtx::collect_arg_nodes(task.node->m_pChild[0], arg_nodes);
                ctx.tasks.push_back({Task::Op::Eval, arg_nodes.at(cond ? 1 : 2).getValue(), 0}, 0);
            }},
            {Task::Op::ExpandRange, [](EvalCtx& ctx, const Task& task) {
                Sequence<Value> list;
                ctx.expand(task.node->getValue().text, list);
                for (auto&& v : list) ctx.values.push_back(v.getValue(), 0);
            }},
            {Task::Op::Call, [](EvalCtx& ctx, const Task& task) {
                Sequence<Value> args;
                for (std::size_t i = task.base; i < ctx.values.size(); ++i)
                    args.push_back(std::move(ctx.values.at(i).value()), 0);
                while (ctx.values.size() > task.base) ctx.values.pop_back();
                ctx.values.push_back(EvalOps::call(task.node->getValue().text, args), 0);
            }},
        };

        EvalCtx ctx{resolve, expand, {}, {}};
        ctx.tasks.push_back({Task::Op::Eval, this->m_root, 0}, 0);

        while (!ctx.tasks.empty()) {
            Task task = ctx.tasks.at(ctx.tasks.size() - 1).getValue();
            ctx.tasks.pop_back();
            if (!task.node) throw ExprError(ExprError::Syntax);
            task_handlers.at(task.op)(ctx, task);
        }

        if (ctx.values.size() != 1) throw ExprError(ExprError::Syntax);
        return std::move(ctx.values.at(ctx.values.size() - 1).value());
    }
};

#endif // !CONTAINERS_EXPRTREE_H
