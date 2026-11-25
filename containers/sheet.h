#ifndef __SHEET_H__
#define __SHEET_H__
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include "vector.h"
#include "../utilities/canvas.h"
#include "../utilities/utf8.h"

class Sheet {
public:
    using Number = double;

private:
    using Cells = Vector<VectorAscTraits<std::string>>;
    using Memo = Vector<VectorAscTraits<Number>>;
    enum State : Ref { Pending, Busy, Done };
    class Formula;

    size_t m_rows, m_cols;
    Cells m_cells;

    size_t index(const std::string& name) const;
    Number evaluate(size_t cell, Memo& memo) const;
    Number compute(size_t cell) const;
    std::string display(size_t cell) const;

public:
    Sheet(size_t rows = 0, size_t cols = 0) : m_rows(rows), m_cols(cols) {
        const size_t limit = 1000000;
        if (rows > limit || cols > limit || rows * cols > limit)
            throw std::length_error("Hoja demasiado grande");
        for (size_t cell = 0; cell < rows * cols; ++cell)
            m_cells.push_back("", cell);
    }

    std::string& operator[](const std::string& name) { return m_cells[index(name)].value(); }
    Number value(const std::string& name) const { return compute(index(name)); }
    std::string text(const std::string& name) const { return display(index(name)); }
    size_t rows() const { return m_rows; }
    size_t cols() const { return m_cols; }
    static std::string column(size_t col);
    static std::string name(size_t row, size_t col) { return column(col) + std::to_string(row + 1); }
    void print(std::ostream& os) const;

    friend std::ostream& operator<<(std::ostream& os, const Sheet& sheet) {
        Cells used;
        for (const auto& cell : sheet.m_cells)
            if (!cell.getValue().empty())
                used.push_back(cell.getValue(), cell.getRef());
        return os << sheet.m_rows << " " << sheet.m_cols << " " << used;
    }

    friend std::istream& operator>>(std::istream& is, Sheet& sheet) {
        size_t rows = 0, cols = 0;
        Cells used;
        if (is >> rows >> cols >> used) {
            Sheet loaded(rows, cols);
            for (const auto& cell : used)
                loaded.m_cells.at(cell.getRef()).value() = cell.getValue();
            sheet = std::move(loaded);
        }
        return is;
    }
};

class Sheet::Formula {
    struct Stats {
        Number sum = 0, min = 0, max = 0;
        size_t count = 0;
        void add(Number number) {
            min = count ? std::min(min, number) : number;
            max = count ? std::max(max, number) : number;
            sum += number;
            ++count;
        }
    };

    const Sheet& m_sheet;
    Memo& m_memo;
    const std::string& m_text;
    size_t m_pos = 1;

    char peek() {
        while (m_pos < m_text.size() && m_text[m_pos] == ' ')
            ++m_pos;
        return m_pos < m_text.size() ? m_text[m_pos] : '\0';
    }
    bool accept(char c) {
        if (peek() != c)
            return false;
        ++m_pos;
        return true;
    }
    void expect(char c) {
        if (!accept(c))
            throw std::runtime_error("#ERROR!");
    }
    static Number divide(Number dividend, Number divisor) {
        if (divisor == 0)
            throw std::runtime_error("#DIV/0!");
        return dividend / divisor;
    }

    std::string name() {
        std::string word;
        for (peek(); m_pos < m_text.size() && std::isalnum(static_cast<unsigned char>(m_text[m_pos])); ++m_pos)
            word += static_cast<char>(std::toupper(static_cast<unsigned char>(m_text[m_pos])));
        return word;
    }
    Number number() {
        const char* start = m_text.c_str() + m_pos;
        char* end = nullptr;
        Number value = std::strtod(start, &end);
        if (end == start)
            throw std::runtime_error("#ERROR!");
        m_pos += end - start;
        return value;
    }

    Number expression() {
        Number value = term();
        while (true) {
            if (accept('+'))
                value += term();
            else if (accept('-'))
                value -= term();
            else
                return value;
        }
    }
    Number term() {
        Number value = factor();
        while (true) {
            if (accept('*'))
                value *= factor();
            else if (accept('/'))
                value = divide(value, factor());
            else
                return value;
        }
    }
    Number factor() {
        if (accept('(')) {
            Number value = expression();
            expect(')');
            return value;
        }
        if (accept('-'))
            return -factor();
        if (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')
            return number();
        std::string word = name();
        if (word.empty())
            throw std::runtime_error("#ERROR!");
        if (accept('('))
            return function(word);
        return m_sheet.evaluate(m_sheet.index(word), m_memo);
    }
    Number function(const std::string& word) {
        Stats stats;
        do
            argument(stats);
        while (accept(','));
        expect(')');
        if (word == "SUMA")
            return stats.sum;
        if (word == "PROMEDIO")
            return divide(stats.sum, stats.count);
        if (word == "MIN")
            return stats.min;
        if (word == "MAX")
            return stats.max;
        throw std::runtime_error("#NOMBRE?");
    }
    void argument(Stats& stats) {
        size_t start = m_pos;
        std::string first = name();
        if (first.empty() || !accept(':')) {
            m_pos = start;
            stats.add(expression());
            return;
        }
        size_t from = m_sheet.index(first), to = m_sheet.index(name()), cols = m_sheet.m_cols;
        size_t top = std::min(from, to) / cols, bottom = std::max(from, to) / cols;
        size_t left = std::min(from % cols, to % cols), right = std::max(from % cols, to % cols);
        for (size_t row = top; row <= bottom; ++row)
            for (size_t col = left; col <= right; ++col)
                if (!m_sheet.m_cells[row * cols + col].getValue().empty())
                    stats.add(m_sheet.evaluate(row * cols + col, m_memo));
    }

public:
    Formula(const Sheet& sheet, Memo& memo, const std::string& text) : m_sheet(sheet), m_memo(memo), m_text(text) {}

    Number evaluate() {
        Number value = expression();
        if (peek() != '\0')
            throw std::runtime_error("#ERROR!");
        return value;
    }
};

inline size_t Sheet::index(const std::string& name) const {
    size_t pos = 0, col = 0, row = 0;
    for (; pos < name.size() && std::isalpha(static_cast<unsigned char>(name[pos])); ++pos)
        col = std::min(col * 26 + (std::toupper(static_cast<unsigned char>(name[pos])) - 'A' + 1), m_cols + 1);
    for (; pos < name.size() && std::isdigit(static_cast<unsigned char>(name[pos])); ++pos)
        row = std::min(row * 10 + (name[pos] - '0'), m_rows + 1);
    if (pos < name.size() || col == 0 || col > m_cols || row == 0 || row > m_rows)
        throw std::runtime_error("#REF!");
    return (row - 1) * m_cols + col - 1;
}

inline auto Sheet::evaluate(size_t cell, Memo& memo) const -> Number {
    auto& slot = memo[cell];
    if (slot.getRef() == Busy)
        throw std::runtime_error("#CICLO!");
    if (slot.getRef() == Pending) {
        slot.ref() = Busy;
        std::string content = m_cells[cell].getValue();
        if (content.starts_with('='))
            slot.value() = Formula(*this, memo, content).evaluate();
        else if (!content.empty()) {
            std::istringstream is(content);
            if (!(is >> slot.value()) || !(is >> std::ws).eof())
                throw std::runtime_error("#VALOR!");
        }
        slot.ref() = Done;
    }
    return slot.getValue();
}

inline auto Sheet::compute(size_t cell) const -> Number {
    Memo memo;
    for (size_t i = 0; i < m_cells.size(); ++i)
        memo.push_back(0, Pending);
    return evaluate(cell, memo);
}

inline std::string Sheet::display(size_t cell) const {
    std::string content = m_cells[cell].getValue();
    if (!content.starts_with('='))
        return content;
    try {
        std::ostringstream os;
        os << std::setprecision(10) << compute(cell);
        return os.str();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
}

inline std::string Sheet::column(size_t col) {
    std::string name;
    for (++col; col > 0; col = (col - 1) / 26)
        name.insert(name.begin(), static_cast<char>('A' + (col - 1) % 26));
    return name;
}

inline void Sheet::print(std::ostream& os) const {
    Canvas canvas;
    size_t x = std::to_string(m_rows).size() + 2;
    for (size_t row = 0; row < m_rows; ++row)
        canvas.print(0, row + 1, row + 1);
    for (size_t col = 0; col < m_cols; ++col) {
        std::string header = column(col);
        size_t width = header.size();
        canvas.print(x, 0, header);
        for (size_t row = 0; row < m_rows; ++row) {
            std::string text = display(row * m_cols + col);
            canvas.print(x, row + 1, text);
            width = std::max(width, Utf8Width(text));
        }
        x += width + 2;
    }
    os << canvas;
}

#endif // __SHEET_H__
