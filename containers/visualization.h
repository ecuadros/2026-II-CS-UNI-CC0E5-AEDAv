#ifndef CONTAINERS_VISUALIZATION_H
#define CONTAINERS_VISUALIZATION_H

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

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

namespace detail {

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
void sideways(std::ostream& os, const V& v, typename V::Handle h, const std::string& prefix, int dir) {
    if (!v.valid(h)) return;
    sideways(os, v, v.child(h, true), prefix + (dir < 0 ? "" : dir == 0 ? "│     " : "      "), 1);
    os << prefix << (dir < 0 ? "" : dir ? "┌──── " : "└──── ");
    v.label(os, h);
    os << '\n';
    sideways(os, v, v.child(h, false), prefix + (dir < 0 ? "" : dir == 1 ? "│     " : "      "), 0);
}

inline void draw_at(std::ostream& os, std::ptrdiff_t x, std::ptrdiff_t y, const std::string& text) {
    os << "\033[u";
    if (y > 0) os << "\033[" << y << 'B';
    if (x > 0) os << "\033[" << x << 'C';
    os << text;
}

template <TreeView V>
std::ptrdiff_t place(std::ostream& os, const V& v, typename V::Handle h, std::ptrdiff_t depth,
                     std::ptrdiff_t& col) {
    if (!v.valid(h)) return -1;
    std::ptrdiff_t lc = place(os, v, v.child(h, false), depth + 1, col);

    std::string text = label(v, h);
    auto len = static_cast<std::ptrdiff_t>(text.size());
    std::ptrdiff_t center = col + len / 2;
    draw_at(os, col, 2 * depth, text);
    col += len + 1;

    std::ptrdiff_t rc = place(os, v, v.child(h, true), depth + 1, col);

    std::ptrdiff_t y = 2 * depth + 1;
    if (lc >= 0) draw_at(os, lc, y, "┌" + repeat("─", center - lc - 1));
    if (rc >= 0) draw_at(os, center + 1, y, repeat("─", rc - center - 1) + "┐");
    if (lc >= 0 || rc >= 0) draw_at(os, center, y, lc >= 0 && rc >= 0 ? "┴" : lc >= 0 ? "┘" : "└");
    return center;
}

struct Block {
    std::vector<std::vector<std::string>> rows;
    std::ptrdiff_t width = 0;
    std::ptrdiff_t mid = 0;
};

template <TreeView V> Block layout(const V& v, typename V::Handle h) {
    std::string text = label(v, h);
    auto len = static_cast<std::ptrdiff_t>(text.size());

    auto lh = v.child(h, false);
    auto rh = v.child(h, true);
    bool hasL = v.valid(lh), hasR = v.valid(rh);
    Block left = hasL ? layout(v, lh) : Block{};
    Block right = hasR ? layout(v, rh) : Block{};

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
        row[start + shift + i] = std::string(1, text[i]);
    b.rows.push_back(row);
    if (!hasL && !hasR) return b;

    row = blank();
    for (std::ptrdiff_t x = hasL ? lm : c; x <= (hasR ? rm : c); ++x)
        row[x + shift] = "─";
    if (hasL) row[lm + shift] = "┌";
    if (hasR) row[rm + shift] = "┐";
    row[c + shift] = hasL && hasR ? "┴" : hasL ? "┘" : "└";
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
    const char* arrow = DoubleLinkedView<C> ? " <-> " : " -> ";
    os << "head";
    for (auto it = c.begin(); it != c.end(); ++it)
        os << arrow << *it;
    if constexpr (CircularView<C>)
        os << arrow << "(vuelve a head)";
    else
        os << arrow << "null";
    os << '\n';
}

template <LinearView C> void print_cells(std::ostream& os, const C& c) {
    std::vector<std::string> cells;
    for (auto it = c.begin(); it != c.end(); ++it)
        cells.push_back(to_text(*it));
    if (cells.empty()) {
        os << "[]\n";
        return;
    }

    std::string top = "┌", middle = "│", bottom = "└", index = " ";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        auto width = static_cast<std::ptrdiff_t>(cells[i].size()) + 2;
        bool last = i + 1 == cells.size();
        top += repeat("─", width) + (last ? "┐" : "┬");
        middle += " " + cells[i] + " │";
        bottom += repeat("─", width) + (last ? "┘" : "┴");

        std::string number = std::to_string(i);
        auto pad = width - static_cast<std::ptrdiff_t>(number.size());
        index += std::string(pad / 2, ' ') + number + std::string(pad - pad / 2 + 1, ' ');
    }
    os << top << '\n' << middle << '\n' << bottom << '\n' << index << '\n';
}

} // namespace detail

template <TreeView V> void print_sideways(std::ostream& os, const V& v) {
    detail::sideways(os, v, v.root(), "", -1);
}

template <TreeView V> void print_tree(std::ostream& os, const V& v) {
    if (!v.valid(v.root())) return;
    for (const auto& cells : detail::layout(v, v.root()).rows) {
        auto last = std::find_if(cells.rbegin(), cells.rend(), [](const std::string& s) { return s != " "; });
        for (auto it = cells.begin(); it != last.base(); ++it)
            os << *it;
        os << '\n';
    }
}

template <TreeView V> void print_cursor(std::ostream& os, const V& v) {
    if (!v.valid(v.root())) return;
    auto rows = static_cast<std::ptrdiff_t>(2 * detail::height(v, v.root()) - 1);
    os << std::string(rows, '\n') << "\033[" << rows << 'A' << "\033[s";
    std::ptrdiff_t col = 0;
    detail::place(os, v, v.root(), 0, col);
    os << "\033[u\033[" << rows << 'B' << '\r' << std::flush;
}

template <LinearView C> void print_linear(std::ostream& os, const C& c) {
    if constexpr (LinkedView<C>)
        detail::print_chain(os, c);
    else
        detail::print_cells(os, c);
}

} // namespace visual

#endif // CONTAINERS_VISUALIZATION_H
