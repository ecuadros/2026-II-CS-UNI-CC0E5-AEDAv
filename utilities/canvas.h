#ifndef __CANVAS_H__
#define __CANVAS_H__
#include <sstream>
#include <string>
#include "../containers/vector.h"

class Canvas {
    Vector<VectorAscTraits<std::string>> m_lines;

public:
    template <typename T>
    void print(size_t x, size_t y, const T& value) {
        std::ostringstream os;
        os << value;
        std::string text = os.str();
        while (m_lines.size() <= y)
            m_lines.push_back("", 0);
        std::string& line = m_lines[y].value();
        if (line.size() < x + text.size())
            line.resize(x + text.size(), ' ');
        line.replace(x, text.size(), text);
    }

    friend std::ostream& operator<<(std::ostream& os, const Canvas& canvas) {
        for (const auto& line : canvas.m_lines)
            os << line.getValue() << "\n";
        return os;
    }
};

#endif // __CANVAS_H__
