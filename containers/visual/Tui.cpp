#include "Tui.h"
#include "../aliases.h"
#include "visualization.h"
#include "../../types.h"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

struct Rect {
    Int x = 0, y = 0, w = 0, h = 0;
};

namespace color {
constexpr std::string_view none = "";
constexpr std::string_view frame = "38;5;103";
constexpr std::string_view frame_on = "1;38;5;48";
constexpr std::string_view title = "1;38;5;183";
constexpr std::string_view title_on = "1;38;5;86";
constexpr std::string_view text = "38;5;253";
constexpr std::string_view dim = "38;5;245";
constexpr std::string_view sel = "1;38;5;231;48;5;62";
constexpr std::string_view key = "1;38;5;75";
constexpr std::string_view value = "1;38;5;87";
constexpr std::string_view accent = "1;38;5;220";
constexpr std::string_view ok = "1;38;5;83";
constexpr std::string_view err = "1;38;5;203";
constexpr std::string_view num = "38;5;156";
constexpr std::string_view str = "38;5;215";
constexpr std::string_view cursor = "7";
constexpr std::string_view palette[] = {"1;38;5;203", "1;38;5;215", "1;38;5;227", "1;38;5;120",
                                        "1;38;5;87",  "1;38;5;111", "1;38;5;177", "1;38;5;212"};
constexpr std::string_view hue(std::size_t i) { return palette[i % std::size(palette)]; }
} // namespace color

constexpr std::string_view k_round[] = {"╭", "╮", "╰", "╯", "─", "│"};
constexpr std::string_view k_heavy[] = {"┏", "┓", "┗", "┛", "━", "┃"};

/** Separa @p s en caracteres UTF-8; cada uno ocupa una columna en pantalla. */
std::vector<std::string_view> glyphs(std::string_view s) {
    std::vector<std::string_view> out;
    for (std::size_t i = 0; i < s.size();) {
        auto c = static_cast<Byte>(s[i]);
        std::size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        n = std::min(n, s.size() - i);
        out.push_back(s.substr(i, n));
        i += n;
    }
    return out;
}

Int width_of(std::string_view s) { return static_cast<Int>(glyphs(s).size()); }

/** Pantalla de celdas (un carácter por celda) que se compone completa y se imprime de una vez. */
class Screen {
public:
    Screen(Int w, Int h) : m_w(w), m_h(h), m_cells(static_cast<std::size_t>(w * h)) {}

    Int w() const { return m_w; }
    Int h() const { return m_h; }

    void put(Int x, Int y, std::string_view g, std::string_view style) {
        if (x < 0 || y < 0 || x >= m_w || y >= m_h) return;
        auto& c = m_cells[static_cast<std::size_t>(y * m_w + x)];
        c.g.assign(g);
        c.style = style;
    }

    /** Escribe @p s en a lo más @p max_w columnas, cortando con "…"; devuelve las columnas usadas. */
    Int text(Int x, Int y, std::string_view s, std::string_view style, Int max_w) {
        if (max_w <= 0) return 0;
        auto gs = glyphs(s);
        Bool cut = static_cast<Int>(gs.size()) > max_w;
        Int n = cut ? max_w : static_cast<Int>(gs.size());
        for (Int i = 0; i < n; ++i)
            put(x + i, y, cut && i == n - 1 ? "…" : gs[static_cast<std::size_t>(i)], style);
        return n;
    }

    void fill(Rect r, std::string_view style = color::none) {
        for (Int y = r.y; y < r.y + r.h; ++y)
            for (Int x = r.x; x < r.x + r.w; ++x)
                put(x, y, " ", style);
    }

    void hline(Int x, Int y, Int w, std::string_view g, std::string_view style) {
        for (Int i = 0; i < w; ++i)
            put(x + i, y, g, style);
    }

    /** Marco con título; el panel con foco usa borde grueso para distinguirse también sin color. */
    void panel(Rect r, std::string_view title, Bool focused, std::string_view hue = color::none) {
        if (r.w < 2 || r.h < 2) return;
        const auto& b = focused ? k_heavy : k_round;
        auto st = hue.empty() ? (focused ? color::frame_on : color::frame) : hue;
        hline(r.x + 1, r.y, r.w - 2, b[4], st);
        hline(r.x + 1, r.y + r.h - 1, r.w - 2, b[4], st);
        for (Int y = r.y + 1; y < r.y + r.h - 1; ++y) {
            put(r.x, y, b[5], st);
            put(r.x + r.w - 1, y, b[5], st);
        }
        put(r.x, r.y, b[0], st);
        put(r.x + r.w - 1, r.y, b[1], st);
        put(r.x, r.y + r.h - 1, b[2], st);
        put(r.x + r.w - 1, r.y + r.h - 1, b[3], st);
        if (!title.empty())
            text(r.x + 2, r.y, " " + std::string(title) + " ", hue.empty() ? (focused ? color::title_on : color::title) : hue, r.w - 4);
    }

    /** Texto alineado a la derecha sobre el borde inferior de @p r. */
    void footer(Rect r, std::string_view s, std::string_view style) {
        Int w = std::min(width_of(s), r.w - 4);
        text(r.x + r.w - 2 - w, r.y + r.h - 1, s, style, w);
    }

    void flush(std::ostream& os) const {
        static const Bool no_color = std::getenv("NO_COLOR") != nullptr;
        std::string out = "\033[H";
        std::string_view cur = "\x01";
        for (Int y = 0; y < m_h; ++y) {
            for (Int x = 0; x < m_w; ++x) {
                const auto& c = m_cells[static_cast<std::size_t>(y * m_w + x)];
                auto st = c.style;
                if (no_color) st = (st == color::sel || st == color::cursor) ? std::string_view("7") : color::none;
                if (st != cur) {
                    out += "\033[0m";
                    if (!st.empty()) out.append("\033[").append(st).append("m");
                    cur = st;
                }
                out += c.g;
            }
            if (y + 1 < m_h) out += "\r\n";
        }
        out += "\033[0m";
        os << out << std::flush;
    }

private:
    struct Cell {
        std::string g = " ";
        std::string_view style;
    };
    Int m_w, m_h;
    std::vector<Cell> m_cells;
};

/** Escribe @p s carácter a carácter, descartando lo que cae fuera de @p clip. */
void put_clipped(Screen& s, Rect clip, Int x, Int y, std::string_view text, std::string_view style) {
    if (y < clip.y || y >= clip.y + clip.h) return;
    for (auto g : glyphs(text)) {
        if (x >= clip.x && x < clip.x + clip.w) s.put(x, y, g, style);
        ++x;
    }
}

void on_winch(Int) {}

/** Terminal en modo raw y pantalla alternativa; restaura todo al salir. */
class TerminalGuard {
public:
    TerminalGuard() {
        tcgetattr(STDIN_FILENO, &m_orig);
        termios raw = m_orig;
        raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        struct sigaction sa {};
        sa.sa_handler = on_winch; // sin SA_RESTART: read() se interrumpe y se redibuja con el nuevo tamaño
        sigemptyset(&sa.sa_mask);
        sigaction(SIGWINCH, &sa, nullptr);
        std::cout << "\033[?1049h\033[?25l" << std::flush;
    }
    ~TerminalGuard() {
        std::cout << "\033[0m\033[?25h\033[?1049l" << std::flush;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig);
    }
    TerminalGuard(const TerminalGuard&) = delete;
    TerminalGuard& operator=(const TerminalGuard&) = delete;

private:
    termios m_orig{};
};

struct KeyEvent {
    enum class Kind { None, Char, Up, Down, Left, Right, Enter, Tab, Backspace, Esc };
    Kind kind = Kind::None;
    Char ch = 0;
};

using K = KeyEvent::Kind;

Bool is_key(const KeyEvent& ev, K kind, Char ch) { return ev.kind == kind || (ev.kind == K::Char && ev.ch == ch); }

KeyEvent read_key() {
    Byte c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) return {};
    if (c == '\r' || c == '\n') return {K::Enter, 0};
    if (c == '\t') return {K::Tab, 0};
    if (c == 127 || c == '\b') return {K::Backspace, 0};
    if (c == 27) {
        termios cur{};
        tcgetattr(STDIN_FILENO, &cur);
        termios nonblock = cur;
        nonblock.c_cc[VMIN] = 0;
        nonblock.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &nonblock);
        Byte seq[2]{};
        ssize_t n1 = read(STDIN_FILENO, &seq[0], 1);
        ssize_t n2 = n1 > 0 ? read(STDIN_FILENO, &seq[1], 1) : 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &cur);
        if (n2 > 0 && (seq[0] == '[' || seq[0] == 'O')) {
            if (seq[1] == 'A') return {K::Up, 0};
            if (seq[1] == 'B') return {K::Down, 0};
            if (seq[1] == 'C') return {K::Right, 0};
            if (seq[1] == 'D') return {K::Left, 0};
        }
        return {K::Esc, 0};
    }
    return {K::Char, static_cast<Char>(c)};
}

std::pair<Int, Int> terminal_size() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) return {ws.ws_col, ws.ws_row};
    return {100, 30};
}

template <typename T> std::string to_str(const T& val) {
    std::ostringstream os;
    os << val;
    return os.str();
}

std::optional<Int> parse_int(const std::string& s) {
    try {
        std::size_t pos = 0;
        Int v = std::stoi(s, &pos);
        while (pos < s.size() && s[pos] == ' ')
            ++pos;
        if (pos == s.size()) return v;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::string col_name(Int col) {
    std::string s;
    for (; col > 0; col = (col - 1) / 26)
        s.insert(s.begin(), static_cast<Char>('A' + (col - 1) % 26));
    return s;
}

std::string cell_name(Int row, Int col) { return col_name(col) + std::to_string(row); }

void empty_hint(Screen& s, Rect a) { s.text(a.x, a.y, "(vacío)  pulsa [a] para agregar elementos", color::dim, a.w); }

/** Copia el dibujo de árbol de `visualization.h` dentro de @p a, desplazado por (@p sx, @p sy). */
template <visual::TreeView V> void draw_tree(Screen& s, Rect a, const V& view, Int sx, Int sy) {
    if (!view.valid(view.root())) return empty_hint(s, a);
    auto canvas = visual::detail::draw(view);
    const auto& rows = canvas.rows();
    for (Int r = 0; r < a.h && sy + r < static_cast<Int>(rows.size()); ++r) {
        const auto& row = rows[static_cast<std::size_t>(sy + r)];
        for (std::size_t x = 0; x < row.size(); ++x)
            if (!row[x].text.empty() && !row[x].blank())
                put_clipped(s, a, a.x + static_cast<Int>(x) - sx, a.y + r, row[x].text, row[x].style);
    }
}

template <class C> std::vector<std::string> values_of(const C& c) {
    std::vector<std::string> items;
    for (auto it = c.begin(); it != c.end(); ++it)
        items.push_back(to_str((*it).getValue()));
    return items;
}

/** Dibuja un arreglo como celdas contiguas con su índice debajo. */
template <class C> void draw_array(Screen& s, Rect a, const C& c, Int sy) {
    auto items = values_of(c);
    if (items.empty()) return empty_hint(s, a);
    Int cw = 3;
    for (const auto& v : items)
        cw = std::max(cw, width_of(v) + 2);
    auto per_row = static_cast<std::size_t>(std::max(1, (a.w - 1) / (cw + 1)));
    Int y = a.y - sy;
    for (std::size_t start = 0; start < items.size(); start += per_row, y += 5) {
        std::size_t n = std::min(items.size() - start, per_row);
        std::string top = "┌", bot = "└";
        for (std::size_t i = 0; i < n; ++i) {
            for (Int k = 0; k < cw; ++k) {
                top += "─";
                bot += "─";
            }
            top += i + 1 < n ? "┬" : "┐";
            bot += i + 1 < n ? "┴" : "┘";
        }
        put_clipped(s, a, a.x, y, top, color::frame);
        put_clipped(s, a, a.x, y + 2, bot, color::frame);
        for (std::size_t i = 0; i < n; ++i) {
            Int x = a.x + static_cast<Int>(i) * (cw + 1);
            const auto& v = items[start + i];
            std::string idx = std::to_string(start + i);
            put_clipped(s, a, x, y + 1, "│", color::frame);
            put_clipped(s, a, x + 1 + (cw - width_of(v)) / 2, y + 1, v, color::hue(start + i));
            put_clipped(s, a, x + 1 + (cw - width_of(idx)) / 2, y + 3, idx, color::dim);
        }
        put_clipped(s, a, a.x + static_cast<Int>(n) * (cw + 1), y + 1, "│", color::frame);
    }
}

/** Dibuja una lista enlazada como nodos en caja unidos por flechas, según su tipo de enlace. */
template <class C> void draw_chain(Screen& s, Rect a, const C& c, Int sy) {
    auto items = values_of(c);
    if (items.empty()) return empty_hint(s, a);
    constexpr std::string_view link = visual::DoubleLinkedView<C> ? " ⇄ " : " → ";
    constexpr std::string_view tail = visual::CircularView<C> ? " ↺ vuelve al inicio" : " → ∅";
    constexpr std::string_view head = "cabeza →  ";
    Int x = a.x, y = a.y - sy;
    Int head_w = width_of(head);
    put_clipped(s, a, x, y + 1, head, color::dim);
    if (x + head_w > a.x + a.w) {
        x = a.x + 2;
        y += 4;
    } else {
        x += head_w;
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        Int vw = width_of(items[i]);
        Int bw = vw + 4;
        std::string_view conn = i + 1 < items.size() ? link : tail;
        std::string_view conn_st = i + 1 < items.size() ? color::frame_on : color::accent;
        Int cw = width_of(conn);
        if (x + bw > a.x + a.w && x > a.x) {
            x = a.x + 2;
            y += 4;
        }
        std::string rule;
        for (Int k = 0; k < vw + 2; ++k)
            rule += "─";
        auto box = color::hue(i);
        put_clipped(s, a, x, y, "┌" + rule + "┐", box);
        put_clipped(s, a, x, y + 1, "│", box);
        put_clipped(s, a, x + 2, y + 1, items[i], box);
        put_clipped(s, a, x + bw - 1, y + 1, "│", box);
        put_clipped(s, a, x, y + 2, "└" + rule + "┘", box);
        x += bw;
        if (x + cw > a.x + a.w && x > a.x) {
            x = a.x + 2;
            y += 4;
        }
        put_clipped(s, a, x, y + 1, conn, conn_st);
        x += cw;
    }
}

struct Msg {
    std::string text;
    Bool ok = true;
};

struct Action {
    Char key;
    std::string label;
    std::string prompt; // vacío: se ejecuta sin pedir texto
};

const Msg bad_int{"ingresa un número entero (ej. 42)", false};

/** Interfaz común de cada container mostrado en la TUI. */
class ContainerTab {
public:
    ContainerTab(std::string name, std::string about, std::string legend = "")
        : m_name(std::move(name)), m_about(std::move(about)), m_legend(std::move(legend)) {}
    virtual ~ContainerTab() = default;

    const std::string& name() const { return m_name; }
    const std::string& about() const { return m_about; }
    const std::string& legend() const { return m_legend; }

    virtual std::string summary() const = 0;
    virtual std::vector<Action> actions() const = 0;
    virtual Msg run(Char key, const std::string& input) = 0;
    virtual void render(Screen& s, Rect area) const = 0;

    virtual Bool navigate(const KeyEvent&) { return false; }
    virtual Char enter_action() const { return 0; }
    virtual std::string initial_input(Char) const { return ""; }

    void scroll(Int dx, Int dy) {
        m_sx = std::max(0, m_sx + dx);
        m_sy = std::max(0, m_sy + dy);
    }

protected:
    Int m_sx = 0;
    Int m_sy = 0;
    Ref m_next_ref = 1;

private:
    std::string m_name, m_about, m_legend;
};

class VectorTab final : public ContainerTab {
public:
    VectorTab() : ContainerTab("Vector", "Arreglo dinámico: acceso por índice O(1), push_back amortizado O(1).") {
        for (Int v : {10, 20, 30, 40})
            m_vec.push_back(v, m_next_ref++);
    }
    std::string summary() const override { return std::to_string(m_vec.size()) + " elem"; }
    std::vector<Action> actions() const override {
        return {{'a', "Agregar al final", "Valor a agregar al final (entero)"},
                {'p', "Quitar el último", ""},
                {'c', "Vaciar", ""}};
    }
    Msg run(Char k, const std::string& in) override {
        if (k == 'a') {
            auto v = parse_int(in);
            if (!v) return bad_int;
            m_vec.push_back(*v, m_next_ref++);
            return {"push_back(" + std::to_string(*v) + ")"};
        }
        if (k == 'p') {
            if (m_vec.size() == 0) return {"el vector ya está vacío", false};
            m_vec.pop_back();
            return {"pop_back()"};
        }
        if (k == 'c') {
            m_vec.clear();
            return {"clear()"};
        }
        return {};
    }
    void render(Screen& s, Rect a) const override { draw_array(s, a, m_vec, m_sy); }

private:
    VectorOf<Int> m_vec;
};

template <typename ListType> class LinkedListTab final : public ContainerTab {
public:
    LinkedListTab(std::string name, std::string about) : ContainerTab(std::move(name), std::move(about)) {
        for (Int v : {15, 5, 30, 20})
            m_list.insert(v, m_next_ref++);
    }
    std::string summary() const override { return std::to_string(values_of(m_list).size()) + " nodos"; }
    std::vector<Action> actions() const override {
        return {{'a', "Insertar (queda ordenado)", "Valor a insertar (entero)"}, {'c', "Vaciar", ""}};
    }
    Msg run(Char k, const std::string& in) override {
        if (k == 'a') {
            auto v = parse_int(in);
            if (!v) return bad_int;
            m_list.insert(*v, m_next_ref++);
            return {"insert(" + std::to_string(*v) + ")"};
        }
        if (k == 'c') {
            m_list.clear();
            return {"clear()"};
        }
        return {};
    }
    void render(Screen& s, Rect a) const override { draw_chain(s, a, m_list, m_sy); }

private:
    ListType m_list;
};

template <typename HeapType> class HeapTab final : public ContainerTab {
public:
    HeapTab(std::string name, std::string about)
        : ContainerTab(std::move(name), std::move(about), "Cada nodo se muestra como (valor, ref).") {
        for (Int v : {40, 15, 30, 8, 25, 50})
            m_heap.push(v, m_next_ref++);
    }
    std::string summary() const override {
        if (m_heap.empty()) return "vacío";
        return std::to_string(m_heap.size()) + " · raíz " + std::to_string(m_heap.top().getValue());
    }
    std::vector<Action> actions() const override {
        return {{'a', "Insertar", "Valor a insertar (entero)"}, {'p', "Extraer la raíz", ""}, {'c', "Vaciar", ""}};
    }
    Msg run(Char k, const std::string& in) override {
        if (k == 'a') {
            auto v = parse_int(in);
            if (!v) return bad_int;
            m_heap.push(*v, m_next_ref++);
            return {"push(" + std::to_string(*v) + ")"};
        }
        if (k == 'p') {
            if (m_heap.empty()) return {"el heap está vacío", false};
            return {"pop() → " + std::to_string(m_heap.pop().getValue())};
        }
        if (k == 'c') {
            m_heap.clear();
            return {"clear()"};
        }
        return {};
    }
    void render(Screen& s, Rect a) const override { draw_tree(s, a, m_heap.tree_view(), m_sx, m_sy); }

private:
    HeapType m_heap;
};

template <typename TreeType> class SearchTreeTab final : public ContainerTab {
public:
    SearchTreeTab(std::string name, std::string about)
        : ContainerTab(std::move(name), std::move(about), "Cada nodo se muestra como (valor, ref).") {
        for (Int v : {50, 30, 70, 20, 40, 60, 80})
            m_tree.insert(v, m_next_ref++);
    }
    std::string summary() const override { return std::to_string(m_tree.size()) + " nodos"; }
    std::vector<Action> actions() const override {
        return {{'a', "Insertar", "Valor a insertar (entero)"},
                {'d', "Eliminar", "Valor a eliminar (entero)"},
                {'f', "Buscar", "Valor a buscar (entero)"},
                {'c', "Vaciar", ""}};
    }
    Msg run(Char k, const std::string& in) override {
        if (k == 'c') {
            m_tree.clear();
            return {"clear()"};
        }
        auto v = parse_int(in);
        if (!v) return bad_int;
        auto n = std::to_string(*v);
        if (k == 'a') {
            m_tree.insert(*v, m_next_ref++);
            return {"insert(" + n + ")"};
        }
        if (k == 'd') return m_tree.remove(*v) ? Msg{"remove(" + n + ")"} : Msg{n + " no está en el árbol", false};
        if (k == 'f') return m_tree.contains(*v) ? Msg{n + " encontrado"} : Msg{n + " no encontrado", false};
        return {};
    }
    void render(Screen& s, Rect a) const override { draw_tree(s, a, m_tree.tree_view(), m_sx, m_sy); }

private:
    TreeType m_tree;
};

class HashTableTab final : public ContainerTab {
public:
    HashTableTab()
        : ContainerTab("HashTable", "Mapa clave → valor ordenado por clave (AVL por dentro): buscar, insertar y borrar en O(log n).") {
        m_table["ana"] = 18;
        m_table["luis"] = 20;
        m_table["maria"] = 16;
        m_table["pedro"] = 14;
    }
    std::string summary() const override { return std::to_string(m_table.size()) + " claves"; }
    std::vector<Action> actions() const override {
        return {{'a', "Guardar clave=valor", "Escribe clave=valor (ej. juan=19)"},
                {'d', "Eliminar clave", "Clave a eliminar"},
                {'f', "Buscar clave", "Clave a buscar"},
                {'c', "Vaciar", ""}};
    }
    Msg run(Char k, const std::string& in) override {
        if (k == 'a') {
            auto eq = in.find('=');
            if (eq == std::string::npos || eq == 0) return {"formato esperado: clave=entero", false};
            auto v = parse_int(in.substr(eq + 1));
            if (!v) return bad_int;
            std::string key = in.substr(0, eq);
            m_table[key] = *v;
            return {"[" + key + "] = " + std::to_string(*v)};
        }
        if (k == 'd') return m_table.remove(in) ? Msg{"eliminada '" + in + "'"} : Msg{"no existe '" + in + "'", false};
        if (k == 'f') {
            auto it = m_table.find(in);
            if (it == m_table.end()) return {"no existe '" + in + "'", false};
            return {in + " → " + std::to_string(it->second)};
        }
        if (k == 'c') {
            m_table.clear();
            return {"clear()"};
        }
        return {};
    }
    void render(Screen& s, Rect a) const override {
        if (m_table.empty()) return empty_hint(s, a);
        Int kw = 5;
        for (const auto& [k, v] : m_table)
            kw = std::max(kw, width_of(k));
        Int y = a.y - m_sy;
        put_clipped(s, a, a.x, y, "Clave", color::title_on);
        put_clipped(s, a, a.x + kw + 1, y, "│ Valor", color::title_on);
        std::string rule;
        for (Int i = 0; i < kw + 1; ++i)
            rule += "─";
        put_clipped(s, a, a.x, y + 1, rule + "┼──────", color::frame);
        y += 2;
        for (const auto& [k, v] : m_table) {
            put_clipped(s, a, a.x, y, k, color::str);
            put_clipped(s, a, a.x + kw + 1, y, "│", color::frame);
            put_clipped(s, a, a.x + kw + 3, y, std::to_string(v), color::num);
            ++y;
        }
    }

private:
    HashTableOf<std::string, Int> m_table;
};

class MerkleTab final : public ContainerTab {
public:
    MerkleTab()
        : ContainerTab("MerkleTree", "Árbol de hashes: cada padre es el hash de sus hijos; cambiar una hoja cambia la raíz. Hash de demostración, no criptográfico.",
                       "Nodos: (hash, nº de hoja); -1 = nodo interno.") {
        for (const Char* b : {"tx1", "tx2", "tx3", "tx4"}) {
            m_merkle.push_back(b);
            m_blocks.emplace_back(b);
        }
    }
    std::string summary() const override { return std::to_string(m_merkle.size()) + " hojas"; }
    std::vector<Action> actions() const override {
        return {{'a', "Agregar bloque", "Texto del nuevo bloque"},
                {'u', "Modificar hoja", "Escribe índice=texto (ej. 0=nuevo)"},
                {'c', "Vaciar", ""}};
    }
    Msg run(Char k, const std::string& in) override {
        if (k == 'a') {
            if (in.empty()) return {"el bloque no puede estar vacío", false};
            m_merkle.push_back(in);
            m_blocks.push_back(in);
            return {"push_back(\"" + in + "\") → nueva raíz " + std::to_string(m_merkle.root_hash())};
        }
        if (k == 'u') {
            auto eq = in.find('=');
            if (eq == std::string::npos) return {"formato esperado: índice=texto", false};
            auto idx = parse_int(in.substr(0, eq));
            if (!idx || *idx < 0 || static_cast<std::size_t>(*idx) >= m_merkle.size())
                return {"índice fuera de rango", false};
            m_merkle.update(static_cast<std::size_t>(*idx), in.substr(eq + 1));
            m_blocks[static_cast<std::size_t>(*idx)] = in.substr(eq + 1);
            return {"update(" + std::to_string(*idx) + ") → nueva raíz " + std::to_string(m_merkle.root_hash())};
        }
        if (k == 'c') {
            m_merkle = MerkleOf<std::string>{};
            m_blocks.clear();
            return {"clear()"};
        }
        return {};
    }
    void render(Screen& s, Rect a) const override {
        if (m_blocks.empty()) return empty_hint(s, a);
        Int x = a.x + s.text(a.x, a.y, "Raíz: ", color::dim, a.w);
        s.text(x, a.y, std::to_string(m_merkle.root_hash()), color::accent, a.x + a.w - x);
        x = a.x + s.text(a.x, a.y + 1, "Bloques: ", color::dim, a.w);
        for (std::size_t i = 0; i < m_blocks.size() && x < a.x + a.w; ++i)
            x += s.text(x, a.y + 1, "[" + std::to_string(i) + "] " + m_blocks[i] + "  ", color::value, a.x + a.w - x);
        draw_tree(s, {a.x, a.y + 3, a.w, a.h - 3}, m_merkle.tree_view(), m_sx, m_sy);
    }

private:
    MerkleOf<std::string> m_merkle;
    std::vector<std::string> m_blocks;
};

class SpreadsheetTab final : public ContainerTab {
public:
    SpreadsheetTab()
        : ContainerTab("Spreadsheet", "Hoja de cálculo: números, texto o fórmulas (=A1+B1, =SUMA(A1:B2), =PROM(A1,B1)).") {
        for (auto [cell, src] : {std::pair{"A1", "10"}, {"A2", "20"}, {"B1", "5"}, {"B2", "=A1+A2*B1"},
                                 {"C1", "=SUMA(A1:B1)"}, {"C2", "=PROM(A1,A2,B1)"}})
            set(cell, src);
    }
    std::string summary() const override { return "celda " + cell_name(m_row, m_col); }
    std::vector<Action> actions() const override {
        return {{'e', "Editar celda (o Enter)", "Editar " + cell_name(m_row, m_col) + ": número, texto o =fórmula"},
                {'d', "Borrar celda", ""},
                {'t', m_show_ast ? "Ocultar árbol de fórmula" : "Mostrar árbol de fórmula", ""},
                {'c', "Vaciar hoja", ""}};
    }
    Char enter_action() const override { return 'e'; }
    std::string initial_input(Char) const override {
        auto it = m_src.find(cell_name(m_row, m_col));
        return it != m_src.end() ? it->second : "";
    }
    Bool navigate(const KeyEvent& ev) override {
        if (is_key(ev, K::Up, 'k')) m_row = std::max(1, m_row - 1);
        else if (is_key(ev, K::Down, 'j')) ++m_row;
        else if (is_key(ev, K::Left, 'h')) m_col = std::max(1, m_col - 1);
        else if (is_key(ev, K::Right, 'l')) ++m_col;
        else return false;
        return true;
    }
    Msg run(Char k, const std::string& in) override {
        std::string target = cell_name(m_row, m_col);
        if (k == 'e') {
            if (in.empty()) return run('d', in);
            try {
                set(target, in);
            } catch (const std::exception& e) {
                return {target + ": " + e.what(), false};
            }
            return {target + " = " + m_sheet.text(target)};
        }
        if (k == 'd') {
            m_src.remove(target);
            return m_sheet.remove(target) ? Msg{target + " borrada"} : Msg{target + " ya estaba vacía", false};
        }
        if (k == 't') {
            m_show_ast = !m_show_ast;
            return {m_show_ast ? "árbol de fórmula visible" : "árbol de fórmula oculto"};
        }
        if (k == 'c') {
            m_sheet = SheetOf<Real>{};
            m_src.clear();
            m_row = m_col = 1;
            return {"hoja vaciada"};
        }
        return {};
    }
    void render(Screen& s, Rect a) const override {
        std::string cur = cell_name(m_row, m_col);
        std::string val = m_sheet.text(cur);
        Int x = a.x + s.text(a.x, a.y, cur, color::accent, a.w);
        x += s.text(x, a.y, " = ", color::dim, a.x + a.w - x);
        x += s.text(x, a.y, val.empty() ? "(vacía)" : val, color::value, a.x + a.w - x);
        if (auto src = initial_input(0); !src.empty() && src[0] == '=') {
            x += s.text(x, a.y, "    fórmula: ", color::dim, a.x + a.w - x);
            s.text(x, a.y, src, color::accent, a.x + a.w - x);
        }

        Rect grid{a.x, a.y + 2, a.w, a.h - 2};
        Rect ast{};
        if (m_show_ast && a.h >= 14) {
            grid.h = (a.h - 2) / 2;
            ast = {a.x, grid.y + grid.h + 1, a.w, a.y + a.h - (grid.y + grid.h + 1)};
        }

        constexpr Int cw = 11, hw = 4;
        Int vis_cols = std::max(1, (grid.w - hw - 1) / (cw + 1));
        Int vis_rows = std::max(1, grid.h - 2);
        Int c0 = std::max(1, m_col - vis_cols + 1);
        Int r0 = std::max(1, m_row - vis_rows + 1);

        s.hline(grid.x, grid.y + 1, hw + vis_cols * (cw + 1), "─", color::frame);
        s.put(grid.x + hw, grid.y, "│", color::frame);
        s.put(grid.x + hw, grid.y + 1, "┼", color::frame);
        for (Int c = 0; c < vis_cols; ++c) {
            Int col = c0 + c, px = grid.x + hw + 1 + c * (cw + 1);
            std::string h = col_name(col);
            s.text(px + (cw - width_of(h)) / 2, grid.y, h, col == m_col ? color::accent : color::title, cw);
            s.put(px + cw, grid.y, "│", color::frame);
            s.put(px + cw, grid.y + 1, c + 1 < vis_cols ? "┼" : "┤", color::frame);
        }
        for (Int r = 0; r < vis_rows; ++r) {
            Int row = r0 + r, py = grid.y + 2 + r;
            std::string n = std::to_string(row);
            s.text(grid.x + hw - 1 - width_of(n), py, n, row == m_row ? color::accent : color::dim, hw - 1);
            s.put(grid.x + hw, py, "│", color::frame);
            for (Int c = 0; c < vis_cols; ++c) {
                Int col = c0 + c, px = grid.x + hw + 1 + c * (cw + 1);
                std::string txt = m_sheet.text(cell_name(row, col));
                Bool is_cur = row == m_row && col == m_col;
                auto st = is_cur ? color::sel : txt.empty() ? color::text : txt[0] == '#' ? color::err : (std::isdigit(static_cast<Byte>(txt[0])) || txt[0] == '-' || txt[0] == '.') ? color::num : color::str;
                if (is_cur) s.fill({px, py, cw, 1}, st);
                s.text(px + 1, py, txt, st, cw - 2);
                s.put(px + cw, py, "│", color::frame);
            }
        }

        if (ast.h > 0) {
            s.hline(ast.x, ast.y - 1, ast.w, "─", color::frame);
            s.text(ast.x + 2, ast.y - 1, " Árbol de la fórmula de " + cur + " ", color::title, ast.w - 4);
            try {
                auto f = m_sheet.formula(cur);
                if (f.empty()) s.text(ast.x, ast.y, "Esta celda no tiene fórmula.", color::dim, ast.w);
                else draw_tree(s, ast, f.tree_view(), 0, 0);
            } catch (const std::exception& e) {
                s.text(ast.x, ast.y, e.what(), color::err, ast.w);
            }
        }
    }

private:
    void set(const std::string& cell, const std::string& src) {
        m_sheet.set(cell, src);
        m_src[cell] = src;
    }

    SheetOf<Real> m_sheet;
    HashTableOf<std::string, std::string> m_src; // texto original de cada celda, para mostrar y reeditar la fórmula
    Int m_row = 1;
    Int m_col = 1;
    Bool m_show_ast = true;
};

struct Group {
    std::string title;
    std::vector<std::unique_ptr<ContainerTab>> tabs;
    std::size_t sel = 0;
};

enum class Mode { Normal, Input, Help };

class App {
public:
    App() {
        m_groups.resize(3);
        auto& lists = m_groups[0];
        lists.title = "Listas";
        lists.tabs.push_back(std::make_unique<VectorTab>());
        lists.tabs.push_back(std::make_unique<LinkedListTab<SortedLinkedList<Int>>>(
            "LinkedList", "Lista enlazada simple y ordenada: cada nodo apunta al siguiente."));
        lists.tabs.push_back(std::make_unique<LinkedListTab<SortedCircularLinkedList<Int>>>(
            "CircularList", "Lista circular ordenada: el último nodo apunta de vuelta al primero."));
        lists.tabs.push_back(std::make_unique<LinkedListTab<SortedDoubleLinkedList<Int>>>(
            "DoubleList", "Lista doblemente enlazada ordenada: cada nodo conoce al anterior y al siguiente."));
        lists.tabs.push_back(std::make_unique<LinkedListTab<SortedCircularDoubleLinkedList<Int>>>(
            "CircularDouble", "Lista doble circular: doble enlace y el último conecta con el primero."));

        auto& trees = m_groups[1];
        trees.title = "Árboles";
        trees.tabs.push_back(std::make_unique<HeapTab<MinHeap<Int>>>(
            "MinHeap", "Montículo mínimo: cada padre es <= que sus hijos; la raíz es el menor."));
        trees.tabs.push_back(std::make_unique<HeapTab<MaxHeap<Int>>>(
            "MaxHeap", "Montículo máximo: cada padre es >= que sus hijos; la raíz es el mayor."));
        trees.tabs.push_back(std::make_unique<SearchTreeTab<BinarySearchTree<Int>>>(
            "BinaryTree", "Árbol binario de búsqueda: izquierda < nodo < derecha."));
        trees.tabs.push_back(std::make_unique<SearchTreeTab<AVLSearchTree<Int>>>(
            "AVL", "BST autobalanceado: las alturas de los subárboles difieren a lo más en 1."));
        trees.tabs.push_back(std::make_unique<SearchTreeTab<BTreeOf<Int>>>(
            "BTree", "Árbol B*: páginas con varias claves ordenadas; reparte o divide al desbordarse."));
        trees.tabs.push_back(std::make_unique<MerkleTab>());

        auto& tables = m_groups[2];
        tables.title = "Tablas";
        tables.tabs.push_back(std::make_unique<HashTableTab>());
        tables.tabs.push_back(std::make_unique<SpreadsheetTab>());

        m_log.push_back({"Bienvenido. Elige un container con ↑↓ y usa las teclas del panel Acciones. ? = ayuda"});
    }

    void run() {
        while (!m_quit) {
            auto [w, h] = terminal_size();
            Screen s(w, h);
            if (w < 60 || h < 18) {
                s.text(1, 1, "Ventana muy pequeña: agrándala a 60x18 o más.", color::accent, w - 2);
                s.text(1, 2, "q para salir", color::dim, w - 2);
                s.flush(std::cout);
                if (is_key(read_key(), K::Esc, 'q')) m_quit = true;
                continue;
            }
            draw(s);
            s.flush(std::cout);
            on_key(read_key());
        }
    }

private:
    std::vector<Group> m_groups;
    std::size_t m_group = 0;
    Bool m_main_focus = false;
    Mode m_mode = Mode::Normal;
    Char m_pending = 0;
    std::string m_prompt, m_input;
    std::deque<Msg> m_log;
    Bool m_quit = false;

    ContainerTab& tab() const {
        const auto& g = m_groups[m_group];
        return *g.tabs[g.sel];
    }

    void log(Msg m) {
        if (m.text.empty()) return;
        m.text = tab().name() + " › " + m.text;
        m_log.push_back(std::move(m));
        if (m_log.size() > 200) m_log.pop_front();
    }

    void execute(Char key, const std::string& input) {
        try {
            log(tab().run(key, input));
        } catch (const std::exception& e) {
            log({e.what(), false});
        }
    }

    void start_action(const Action& a) {
        if (a.prompt.empty()) return execute(a.key, "");
        m_mode = Mode::Input;
        m_pending = a.key;
        m_prompt = a.prompt;
        m_input = tab().initial_input(a.key);
    }

    void draw(Screen& s) {
        Int body_h = s.h() - 1;
        Int side_w = std::clamp(s.w() * 3 / 10, 28, 40);
        Int log_h = body_h >= 28 ? 7 : 5;
        draw_sidebar(s, {0, 0, side_w, body_h});
        draw_main(s, {side_w, 0, s.w() - side_w, body_h - log_h});
        draw_log(s, {side_w, body_h - log_h, s.w() - side_w, log_h});
        draw_keybar(s, s.h() - 1);
        if (m_mode == Mode::Input) draw_input(s);
        if (m_mode == Mode::Help) draw_help(s);
    }

    void draw_sidebar(Screen& s, Rect r) {
        auto actions = tab().actions();
        Int need = static_cast<Int>(actions.size()) + 2;
        for (const auto& g : m_groups)
            need += static_cast<Int>(g.tabs.size()) + 2;
        Bool collapse = need > r.h;
        Int y = r.y;
        for (std::size_t gi = 0; gi < m_groups.size(); ++gi) {
            const auto& g = m_groups[gi];
            Bool active = gi == m_group;
            Bool expanded = !collapse || active;
            Int h = expanded ? static_cast<Int>(g.tabs.size()) + 2 : 3;
            Rect pr{r.x, y, r.w, h};
            s.panel(pr, "[" + std::to_string(gi + 1) + "] " + g.title, active && !m_main_focus, color::hue(gi * 3 + 1));
            s.footer(pr, std::to_string(g.sel + 1) + " de " + std::to_string(g.tabs.size()), color::dim);
            for (std::size_t i = 0; i < g.tabs.size(); ++i) {
                if (!expanded && i != g.sel) continue;
                Int ry = expanded ? y + 1 + static_cast<Int>(i) : y + 1;
                Bool shown = active && i == g.sel;
                auto st = shown ? color::sel : color::text;
                if (shown) s.fill({r.x + 1, ry, r.w - 2, 1}, st);
                Int x = r.x + 1 + s.text(r.x + 1, ry, shown ? " ▶ " : "   ", st, 3);
                x += s.text(x, ry, g.tabs[i]->name(), st, r.w - 5);
                std::string sum = g.tabs[i]->summary();
                Int room = r.x + r.w - 3 - x;
                if (room > 4) {
                    Int sw = std::min(width_of(sum), room);
                    s.text(r.x + r.w - 2 - sw, ry, sum, shown ? st : color::dim, sw);
                }
            }
            y += h;
        }
        Rect ar{r.x, y, r.w, r.y + r.h - y};
        s.panel(ar, "Acciones", false, color::hue(6));
        for (std::size_t i = 0; i < actions.size() && static_cast<Int>(i) < ar.h - 2; ++i) {
            Int ay = ar.y + 1 + static_cast<Int>(i);
            s.text(ar.x + 2, ay, std::string(1, actions[i].key), color::key, 1);
            s.text(ar.x + 5, ay, actions[i].label, color::text, ar.w - 7);
        }
    }

    void draw_main(Screen& s, Rect r) {
        const auto& t = tab();
        s.panel(r, t.name() + " · " + t.summary(), m_main_focus);
        s.footer(r, m_main_focus ? " Esc: volver a la lista " : " Enter: entrar al panel ", color::dim);
        Rect in{r.x + 2, r.y + 1, r.w - 4, r.h - 2};
        s.text(in.x, in.y, t.about(), color::dim, in.w);
        Int top = 1;
        if (!t.legend().empty()) {
            s.text(in.x, in.y + 1, t.legend(), color::dim, in.w);
            top = 2;
        }
        t.render(s, {in.x, in.y + top + 1, in.w, in.h - top - 1});
    }

    void draw_log(Screen& s, Rect r) {
        s.panel(r, "Registro", false);
        Int lines = r.h - 2;
        Int start = std::max(0, static_cast<Int>(m_log.size()) - lines);
        for (Int i = start; i < static_cast<Int>(m_log.size()); ++i) {
            const auto& m = m_log[static_cast<std::size_t>(i)];
            Int y = r.y + 1 + i - start;
            s.text(r.x + 2, y, m.ok ? "✓" : "✗", m.ok ? color::ok : color::err, 1);
            s.text(r.x + 4, y, m.text, m.ok ? color::text : color::err, r.w - 6);
        }
    }

    void draw_keybar(Screen& s, Int y) {
        std::vector<std::pair<std::string, std::string>> keys;
        if (m_mode == Mode::Input) keys = {{"Enter", "confirmar"}, {"Esc", "cancelar"}, {"Retroceso", "borrar"}};
        else if (m_mode == Mode::Help) keys = {{"Esc", "cerrar ayuda"}};
        else if (m_main_focus)
            keys = {{"←↑↓→", tab().enter_action() ? "mover celda" : "desplazar"}, {"Esc", "volver a la lista"},
                    {"?", "ayuda"}, {"q", "salir"}};
        else
            keys = {{"↑↓", "elegir container"}, {"←→", "cambiar grupo"}, {"Enter", "entrar al panel"},
                    {"?", "ayuda"}, {"q", "salir"}};
        Int x = 1;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            Int need = width_of(keys[i].second) + width_of(keys[i].first) + 5;
            if (x + need > s.w()) break;
            if (i > 0) x += s.text(x, y, " | ", color::frame, 3);
            x += s.text(x, y, keys[i].second + ": ", color::dim, s.w() - x);
            x += s.text(x, y, keys[i].first, color::key, s.w() - x);
        }
    }

    void draw_input(Screen& s) {
        Int w = std::min(72, s.w() - 6), h = 5;
        Rect r{(s.w() - w) / 2, (s.h() - h) / 2, w, h};
        s.fill(r);
        s.panel(r, m_prompt, true);
        s.footer(r, " Enter: confirmar · Esc: cancelar ", color::dim);
        Int x = r.x + 2 + s.text(r.x + 2, r.y + 2, "› ", color::accent, 2);
        auto gs = glyphs(m_input);
        auto room = static_cast<std::size_t>(r.w - 7);
        std::string shown;
        for (std::size_t i = gs.size() > room ? gs.size() - room : 0; i < gs.size(); ++i)
            shown += gs[i];
        x += s.text(x, r.y + 2, shown, color::text, static_cast<Int>(room));
        s.put(x, r.y + 2, " ", color::cursor);
    }

    void draw_help(Screen& s) {
        std::vector<std::pair<std::string, std::string>> rows = {
            {"", "Navegación"},
            {"↑ ↓  j k", "elegir container"},
            {"← →  h l", "cambiar de grupo (Listas, Árboles, Tablas)"},
            {"1 2 3", "saltar a un grupo"},
            {"Enter  0", "entrar al panel principal"},
            {"←↑↓→", "dentro del panel: desplazar / mover celda"},
            {"Esc", "volver a la lista"},
            {"Tab", "alternar entre la lista y el panel"},
            {"q", "salir"},
            {"", ""},
            {"", "Acciones de " + tab().name()},
        };
        for (const auto& a : tab().actions())
            rows.push_back({std::string(1, a.key), a.label});
        Int w = std::min(64, s.w() - 4);
        Int h = std::min(static_cast<Int>(rows.size()) + 4, s.h() - 2);
        Rect r{(s.w() - w) / 2, (s.h() - h) / 2, w, h};
        s.fill(r);
        s.panel(r, "Ayuda", true);
        s.footer(r, " Esc: cerrar ", color::dim);
        for (Int i = 0; i < static_cast<Int>(rows.size()) && i < h - 4; ++i) {
            Int y = r.y + 2 + i;
            const auto& [k, d] = rows[static_cast<std::size_t>(i)];
            if (k.empty()) s.text(r.x + 2, y, d, color::accent, r.w - 4);
            else {
                s.text(r.x + 4, y, k, color::key, 10);
                s.text(r.x + 16, y, d, color::text, r.w - 18);
            }
        }
    }

    void move_selection(Int delta) {
        auto& g = m_groups[m_group];
        Int next = static_cast<Int>(g.sel) + delta;
        if (next >= 0 && next < static_cast<Int>(g.tabs.size())) {
            g.sel = static_cast<std::size_t>(next);
            return;
        }
    }

    void on_key(const KeyEvent& ev) {
        if (m_mode == Mode::Input) {
            if (ev.kind == K::Esc) m_mode = Mode::Normal;
            else if (ev.kind == K::Enter) {
                m_mode = Mode::Normal;
                execute(m_pending, m_input);
            } else if (ev.kind == K::Backspace) {
                while (!m_input.empty() && (static_cast<Byte>(m_input.back()) & 0xC0) == 0x80)
                    m_input.pop_back();
                if (!m_input.empty()) m_input.pop_back();
            } else if (ev.kind == K::Char && static_cast<Byte>(ev.ch) >= 32) m_input.push_back(ev.ch);
            return;
        }
        if (m_mode == Mode::Help) {
            if (ev.kind == K::Esc || ev.kind == K::Enter || (ev.kind == K::Char && (ev.ch == '?' || ev.ch == 'q')))
                m_mode = Mode::Normal;
            return;
        }

        if (ev.kind == K::Char) {
            if (ev.ch == 'q' || ev.ch == 3) {
                m_quit = true;
                return;
            }
            if (ev.ch == '?') {
                m_mode = Mode::Help;
                return;
            }
            if (ev.ch == '0') {
                m_main_focus = true;
                return;
            }
            if (ev.ch >= '1' && ev.ch < static_cast<Char>('1' + m_groups.size())) {
                m_group = static_cast<std::size_t>(ev.ch - '1');
                m_main_focus = false;
                return;
            }
            for (const auto& a : tab().actions())
                if (a.key == ev.ch) return start_action(a);
        }
        if (ev.kind == K::Tab) {
            m_main_focus = !m_main_focus;
            return;
        }

        if (m_main_focus) {
            if (ev.kind == K::Esc) m_main_focus = false;
            else if (ev.kind == K::Enter) {
                for (const auto& a : tab().actions())
                    if (a.key == tab().enter_action()) return start_action(a);
            } else if (!tab().navigate(ev)) {
                if (is_key(ev, K::Left, 'h')) tab().scroll(-4, 0);
                else if (is_key(ev, K::Right, 'l')) tab().scroll(4, 0);
                else if (is_key(ev, K::Up, 'k')) tab().scroll(0, -2);
                else if (is_key(ev, K::Down, 'j')) tab().scroll(0, 2);
            }
            return;
        }

        if (is_key(ev, K::Up, 'k')) move_selection(-1);
        else if (is_key(ev, K::Down, 'j')) move_selection(1);
        else if (ev.kind == K::Enter) m_main_focus = true;
    }
};

} // namespace

void RunTUI() {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        std::cerr << "La TUI requiere una terminal interactiva (TTY).\n";
        return;
    }
    TerminalGuard guard;
    App().run();
}
