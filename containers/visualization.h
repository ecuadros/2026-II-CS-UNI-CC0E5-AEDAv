#ifndef CONTAINERS_VISUALIZATION_H
#define CONTAINERS_VISUALIZATION_H

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>

namespace visual {

template <class V>
concept TreeView = requires(const V& v, typename V::Handle h, std::ostream& os) {
    { v.root() } -> std::same_as<typename V::Handle>;
    { v.child(h, true) } -> std::same_as<typename V::Handle>;
    { v.valid(h) } -> std::convertible_to<bool>;
    v.label(os, h);
};

template <class C>
concept HasTreeView = requires(const C& c) {
    { c.tree_view() } -> TreeView;
};

template <class C>
concept LinearView = requires(const C& c, std::ostream& os) {
    c.begin();
    c.end();
    os << *c.begin();
};

template <class C>
concept LinkedView = LinearView<C> && requires(const typename C::Node& node) { node.m_pNext; };

template <class C>
concept DoubleLinkedView = LinkedView<C> && requires(const typename C::Node& node) { node.m_pPrev; };

template <class C>
concept CircularView = LinkedView<C> && C::is_circular;

enum class ColorMode { Auto, Always, Never };

enum class Layout { Vertical, Sideways, Cursor };

inline ColorMode color_mode = ColorMode::Auto;

namespace style {
inline constexpr std::string_view edge = "2;37";
inline constexpr std::string_view value = "1;36";
inline constexpr std::string_view index = "2;33";
inline constexpr std::string_view head = "1;32";
inline constexpr std::string_view end = "1;31";
inline constexpr std::string_view loop = "1;35";
inline constexpr std::string_view levels[] = {"1;93", "1;96", "1;92", "1;95", "1;94", "1;91"};
} // namespace style

class Painter {
public:
    explicit Painter(const std::ostream& os) : m_on(enabled(os)) {}

    std::string operator()(std::string_view code, std::string_view text) const {
        if (!m_on || text.empty()) return std::string(text);
        return "\033[" + std::string(code) + 'm' + std::string(text) + "\033[0m";
    }

    std::string level(std::size_t depth, std::string_view text) const {
        return (*this)(style::levels[depth % std::size(style::levels)], text);
    }

private:
    static bool enabled(const std::ostream& os) {
        if (color_mode != ColorMode::Auto) return color_mode == ColorMode::Always;
        if (std::getenv("NO_COLOR")) return false;
        return (&os == &std::cout && isatty(fileno(stdout))) || (&os == &std::cerr && isatty(fileno(stderr)));
    }

    bool m_on;
};

inline std::string repeat(const std::string& s, std::ptrdiff_t n) {
    std::string out;
    for (std::ptrdiff_t i = 0; i < n; ++i)
        out += s;
    return out;
}

template <class T> std::string to_text(const T& value) {
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

namespace detail {

template <TreeView V> std::string label(const V& v, typename V::Handle h) {
    std::ostringstream ss;
    v.label(ss, h);
    return ss.str();
}

template <TreeView V> std::size_t height(const V& v, typename V::Handle h) {
    if constexpr (requires { v.height(h); }) {
        return v.height(h);
    } else {
        if (!v.valid(h)) return 0;
        return 1 + std::max(height(v, v.child(h, false)), height(v, v.child(h, true)));
    }
}

template <TreeView V>
void sideways(std::ostream& os, const Painter& paint, const V& v, typename V::Handle h, const std::string& prefix,
              int dir, std::size_t depth) {
    if (!v.valid(h)) return;
    sideways(os, paint, v, v.child(h, true), prefix + (dir < 0 ? "" : dir == 0 ? "│     " : "      "), 1, depth + 1);
    os << paint(style::edge, prefix + (dir < 0 ? "" : dir ? "┌──── " : "└──── "));
    os << paint.level(depth, label(v, h)) << '\n';
    sideways(os, paint, v, v.child(h, false), prefix + (dir < 0 ? "" : dir == 1 ? "│     " : "      "), 0, depth + 1);
}

inline void draw_at(std::ostream& os, std::ptrdiff_t x, std::ptrdiff_t y, const std::string& text) {
    os << "\033[u";
    if (y > 0) os << "\033[" << y << 'B';
    if (x > 0) os << "\033[" << x << 'C';
    os << text;
}

template <TreeView V>
std::ptrdiff_t place(std::ostream& os, const Painter& paint, const V& v, typename V::Handle h, std::ptrdiff_t depth,
                     std::ptrdiff_t& col) {
    if (!v.valid(h)) return -1;
    std::ptrdiff_t lc = place(os, paint, v, v.child(h, false), depth + 1, col);

    std::string text = label(v, h);
    auto len = static_cast<std::ptrdiff_t>(text.size());
    std::ptrdiff_t center = col + len / 2;
    draw_at(os, col, 2 * depth, paint.level(depth, text));
    col += len + 1;

    std::ptrdiff_t rc = place(os, paint, v, v.child(h, true), depth + 1, col);

    std::ptrdiff_t y = 2 * depth + 1;
    if (lc >= 0) draw_at(os, lc, y, paint(style::edge, "┌" + repeat("─", center - lc - 1)));
    if (rc >= 0) draw_at(os, center + 1, y, paint(style::edge, repeat("─", rc - center - 1) + "┐"));
    if (lc >= 0 || rc >= 0)
        draw_at(os, center, y, paint(style::edge, lc >= 0 && rc >= 0 ? "┴" : lc >= 0 ? "┘" : "└"));
    return center;
}

struct Block {
    std::vector<std::vector<std::string>> rows;
    std::ptrdiff_t width = 0;
    std::ptrdiff_t mid = 0;
};

template <TreeView V> Block layout(const Painter& paint, const V& v, typename V::Handle h, std::size_t depth = 0) {
    std::string text = label(v, h);
    auto len = static_cast<std::ptrdiff_t>(text.size());

    auto lh = v.child(h, false);
    auto rh = v.child(h, true);
    bool hasL = v.valid(lh), hasR = v.valid(rh);
    Block left = hasL ? layout(paint, v, lh, depth + 1) : Block{};
    Block right = hasR ? layout(paint, v, rh, depth + 1) : Block{};

    std::ptrdiff_t roff = left.width + (hasL && hasR ? 3 : 0);
    std::ptrdiff_t lm = left.mid, rm = roff + right.mid;
    std::ptrdiff_t c = hasL && hasR ? (lm + rm) / 2 : hasL ? lm + 3 : hasR ? rm - 3 : len / 2;
    std::ptrdiff_t start = c - len / 2;
    std::ptrdiff_t shift = std::max<std::ptrdiff_t>(0, -start);

    Block b;
    b.width = std::max(roff + right.width, start + len) + shift;
    b.mid = c + shift;
    auto blank = [&] { return std::vector<std::string>(b.width, " "); };

    auto row = blank();
    for (std::ptrdiff_t i = 0; i < len; ++i)
        row[start + shift + i] = "";
    if (len > 0) row[start + shift] = paint.level(depth, text);
    b.rows.push_back(row);
    if (!hasL && !hasR) return b;

    row = blank();
    for (std::ptrdiff_t x = hasL ? lm : c; x <= (hasR ? rm : c); ++x)
        row[x + shift] = paint(style::edge, "─");
    if (hasL) row[lm + shift] = paint(style::edge, "┌");
    if (hasR) row[rm + shift] = paint(style::edge, "┐");
    row[c + shift] = paint(style::edge, hasL && hasR ? "┴" : hasL ? "┘" : "└");
    b.rows.push_back(row);

    for (std::size_t i = 0; i < std::max(left.rows.size(), right.rows.size()); ++i) {
        row = blank();
        if (i < left.rows.size())
            std::copy(left.rows[i].begin(), left.rows[i].end(), row.begin() + shift);
        if (i < right.rows.size())
            std::copy(right.rows[i].begin(), right.rows[i].end(), row.begin() + roff + shift);
        b.rows.push_back(row);
    }
    return b;
}

template <LinearView C> void print_chain(std::ostream& os, const C& c) {
    Painter paint(os);
    std::string arrow = paint(style::edge, DoubleLinkedView<C> ? " <-> " : " -> ");
    os << paint(style::head, "head");
    for (auto it = c.begin(); it != c.end(); ++it)
        os << arrow << paint(style::value, to_text(*it));
    if constexpr (CircularView<C>)
        os << arrow << paint(style::loop, "(vuelve a head)");
    else
        os << arrow << paint(style::end, "null");
    os << '\n';
}

template <LinearView C> void print_cells(std::ostream& os, const C& c) {
    Painter paint(os);
    std::vector<std::string> cells;
    for (auto it = c.begin(); it != c.end(); ++it)
        cells.push_back(to_text(*it));
    if (cells.empty()) {
        os << paint(style::edge, "[]") << '\n';
        return;
    }

    std::string bar = paint(style::edge, "│");
    std::string top = "┌", middle = bar, bottom = "└", index = " ";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        auto width = static_cast<std::ptrdiff_t>(cells[i].size()) + 2;
        bool last = i + 1 == cells.size();
        top += repeat("─", width) + (last ? "┐" : "┬");
        middle += " " + paint(style::value, cells[i]) + " " + bar;
        bottom += repeat("─", width) + (last ? "┘" : "┴");

        std::string number = std::to_string(i);
        auto pad = width - static_cast<std::ptrdiff_t>(number.size());
        index += std::string(pad / 2, ' ') + number + std::string(pad - pad / 2 + 1, ' ');
    }
    os << paint(style::edge, top) << '\n'
       << middle << '\n'
       << paint(style::edge, bottom) << '\n'
       << paint(style::index, index) << '\n';
}

template <TreeView V> void print_sideways(std::ostream& os, const V& v) {
    sideways(os, Painter(os), v, v.root(), "", -1, 0);
}

template <TreeView V> void print_vertical(std::ostream& os, const V& v) {
    for (const auto& cells : layout(Painter(os), v, v.root()).rows) {
        auto last = std::find_if(cells.rbegin(), cells.rend(), [](const std::string& s) { return s != " "; });
        for (auto it = cells.begin(); it != last.base(); ++it)
            os << *it;
        os << '\n';
    }
}

template <TreeView V> void print_cursor(std::ostream& os, const V& v) {
    auto rows = static_cast<std::ptrdiff_t>(2 * height(v, v.root()) - 1);
    os << std::string(rows, '\n') << "\033[" << rows << 'A' << "\033[s";
    std::ptrdiff_t col = 0;
    place(os, Painter(os), v, v.root(), 0, col);
    os << "\033[u\033[" << rows << 'B' << '\r' << std::flush;
}

} // namespace detail

template <TreeView V> void print_tree(std::ostream& os, const V& v, Layout layout = Layout::Vertical) {
    if (!v.valid(v.root())) return;
    switch (layout) {
    case Layout::Vertical: detail::print_vertical(os, v); break;
    case Layout::Sideways: detail::print_sideways(os, v); break;
    case Layout::Cursor: detail::print_cursor(os, v); break;
    }
}

template <LinearView C> void print_linear(std::ostream& os, const C& c) {
    if constexpr (LinkedView<C>)
        detail::print_chain(os, c);
    else
        detail::print_cells(os, c);
}

} // namespace visual

#endif // CONTAINERS_VISUALIZATION_H
