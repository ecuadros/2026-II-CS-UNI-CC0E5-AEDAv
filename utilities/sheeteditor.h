#ifndef __SHEETEDITOR_H__
#define __SHEETEDITOR_H__
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include "canvas.h"
#include "terminal.h"
#include "utf8.h"
#include "../containers/sheet.h"

class SheetEditor {
    Sheet& m_sheet;
    std::string m_file, m_input, m_message;
    size_t m_row = 0, m_col = 0, m_home = 0;
    bool m_editing = false, m_modified = false;

    std::string& content() { return m_sheet[Sheet::name(m_row, m_col)]; }

    static std::string clip(const std::string& text, size_t width) {
        return Utf8Width(text) > width ? text.substr(0, Utf8Offset(text, width - 1)) + "~" : text;
    }
    void draw();

    void write(const std::string& text) {
        m_modified = m_modified || text != content();
        content() = text;
        m_editing = false;
    }
    void edit(const std::string& text) {
        m_input = text;
        m_editing = true;
    }
    void accept() {
        if (m_editing)
            write(m_input);
    }
    void move(TX key) {
        accept();
        if (key == Terminal::Up && m_row > 0)
            --m_row;
        if (key == Terminal::Down && m_row + 1 < m_sheet.rows())
            ++m_row;
        if (key == Terminal::Left && m_col > 0)
            --m_col;
        if (key == Terminal::Right && m_col + 1 < m_sheet.cols())
            ++m_col;
    }
    void save() {
        accept();
        std::ofstream file(m_file);
        if (file << m_sheet << std::endl) {
            m_modified = false;
            m_message = "Guardado en " + m_file;
        } else {
            m_message = "No se pudo guardar en " + m_file;
        }
    }
    void type(TX key) {
        if (!m_editing)
            edit("");
        m_input += static_cast<char>(key);
    }
    void press(TX key);

public:
    SheetEditor(Sheet& sheet, const std::string& file) : m_sheet(sheet), m_file(file) {}
    void run();
};

inline void SheetEditor::draw() {
    const size_t width = 12, header = 2, footer = 6;
    size_t margin = std::to_string(m_sheet.rows()).size() + 2;
    winsize screen = Terminal::size();
    size_t rows = std::clamp<size_t>(std::max<size_t>(screen.ws_row, header + footer) - header - footer, 1, m_sheet.rows());
    size_t cols = std::clamp<size_t>((std::max<size_t>(screen.ws_col, margin) - margin) / width, 1, m_sheet.cols());
    size_t top = m_row < rows ? 0 : m_row - rows + 1;
    size_t left = m_col < cols ? 0 : m_col - cols + 1;

    Canvas canvas;
    canvas.print(0, 0, Sheet::name(m_row, m_col) + ": " + (m_editing ? m_input + "_" : content()));
    for (size_t col = left; col < left + cols; ++col) {
        size_t x = margin + (col - left) * width;
        canvas.print(x, header, Sheet::column(col));
        for (size_t row = top; row < top + rows; ++row) {
            size_t y = header + 1 + row - top;
            bool selected = row == m_row && col == m_col;
            std::string text = clip(selected && m_editing ? m_input : m_sheet.text(Sheet::name(row, col)), width - 1);
            canvas.print(0, y, row + 1);
            if (selected)
                canvas.print(x - 1, y, "[" + text + "]");
            else
                canvas.print(x, y, text);
        }
    }
    canvas.print(0, header + rows + 2, "Flechas, Tab: mover   Enter: editar o aceptar   Esc: cancelar   Supr: borrar");
    canvas.print(0, header + rows + 3, "Ctrl+S: guardar   Ctrl+Q: salir");
    canvas.print(0, header + rows + 4, m_message);
    Terminal::clear();
    std::cout << canvas << std::flush;
}

inline void SheetEditor::press(TX key) {
    switch (key) {
    case Terminal::Up:
    case Terminal::Down:
    case Terminal::Left:
    case Terminal::Right:
        move(key);
        m_home = m_col;
        break;
    case Terminal::Tab:
        move(Terminal::Right);
        break;
    case Terminal::Enter:
        if (m_editing) {
            move(Terminal::Down);
            m_col = m_home;
        } else {
            edit(content());
        }
        break;
    case Terminal::Escape:
        m_editing = false;
        break;
    case Terminal::Backspace:
        if (m_editing && !m_input.empty())
            m_input.resize(Utf8Offset(m_input, Utf8Width(m_input) - 1));
        break;
    case Terminal::Delete:
        if (!m_editing)
            write("");
        break;
    case Terminal::Save:
        save();
        break;
    default:
        if (key >= ' ' && key < Terminal::Up)
            type(key);
    }
}

inline void SheetEditor::run() {
    Terminal terminal;
    bool leaving = false;
    for (TX key = Terminal::Unknown; key != Terminal::Closed;) {
        draw();
        key = Terminal::read_key();
        m_message = "";
        if (key == Terminal::Quit) {
            accept();
            if (leaving || !m_modified)
                return;
            m_message = "Hay cambios sin guardar. Ctrl+S: guardar   Ctrl+Q otra vez: salir sin guardar";
        } else {
            press(key);
        }
        leaving = key == Terminal::Quit;
    }
}

#endif // __SHEETEDITOR_H__
