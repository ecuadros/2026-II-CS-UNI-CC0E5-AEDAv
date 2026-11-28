/** @file
 * Demos del lienzo y de la hoja de cálculo.
 */

#include "utils/generic_tests.h"

void DemoCanvas() {
    PrintColor("DemoCanvas");
    visual::detail::Canvas canvas;
    canvas.label(0, 0, "origen (0,0)", visual::style::head);
    canvas.label(20, 2, "texto en (20,2)", visual::style::value);
    canvas.label(5, 4, "texto en (5,4)", visual::style::loop);
    canvas.hline(0, 30, 6, "─", visual::style::edge);
    canvas.glyph(15, 6, "●", visual::style::end);
    canvas.print(std::cout, visual::Painter(std::cout));
}

void PrintSerializedSheet(const std::string& serialized, std::ostream& os);

void DemoSheet() {
    PrintColor("DemoSheet (hoja de calculo de texto)");
    const std::string name = "Sheet";
    SheetOf<> sheet;
    sheet.set("A1", "10");
    sheet.set("A2", "20");
    sheet.set("A3", "30");
    sheet.set("B1", "=(A1+A2)*2 - -A3/3");
    sheet.set("B2", "=SUMA(A1:A3, 4)");
    sheet.set("B3", "=PROM(A1:A3) + MAX(A1, A2) - MIN(A1:A3) + CONTAR(A1:B2)");
    sheet.set("C1", "Total");
    sheet.set("C2", "=2^3^2");
    sheet.set("C3", "=A1/(A2-20)");
    sheet.set("D1", "=D2");
    sheet.set("D2", "=D1+1");
    sheet.set("D3", "=NADA(1)");
    Check(sheet.value("B1") == 70, name, "parentesis o menos unario");
    Check(sheet.value("b2") == 64, name, "SUMA con rango");
    Check(sheet.value("B3") == 20 + 20 - 10 + 4, name, "PROM/MAX/MIN/CONTAR");
    Check(sheet.value("C2") == 512, name, "^ asocia a la derecha");
    Check(sheet.text("C1") == "Total" && sheet.text("C3") == "#DIV/0" && sheet.text("D1") == "#CICLO" &&
              sheet.text("D3") == "#NAME" && sheet.text("Z9").empty(),
          name, "codigos de error");
    sheet.set("A1", "0");
    Check(sheet.value("B1") == 50, name, "el cambio no se propago");

    sheet.set("G1", "=SI(A2>15, \"Alto\", \"Bajo\")");
    sheet.set("G2", "=CONCAT(C1, \": \", B1)");
    sheet.set("G3", "=LARGO(C1) + SI(A1=0, 10, 0)");
    Check(sheet.text("G1") == "Alto" && sheet.text("G2") == "Total: 50" && sheet.value("G3") == 15, name,
          "SI, comparaciones y cadenas");

    auto postorder = [](const SheetOf<>::Formula& tree) {
        std::string order;
        for (const auto& node : tree.PostOrder()) order += node.getValue().text + " ";
        return order;
    };
    using Formula = SheetOf<>::Formula;
    Check(postorder(Formula("(A1+A2)*2")) == "A1 A2 + 2 * " && postorder(Formula("A1+A2*2")) == "A1 A2 2 * + ",
          name, "los parentesis no cambiaron la forma del AST");
    Check(postorder(Formula("suma(A1:A3, 4, -1)")) == "A1:A3 4 1 - , , SUMA " && Formula("2^3^2").size() == 5 &&
              sheet.formula("C1").empty(),
          name, "AST de funciones o potencias");
    for (std::string bad : {"(1+2", "1 2", "SUMA(A1:)", "A1:A3", "1+#"}) {
        Bool threw = false;
        try {
            Formula tree(bad);
        } catch (const SheetError& error) {
            threw = std::string(error.what()) == "#SYNTAX";
        }
        Check(threw, name, "\"" + bad + "\" debio dar #SYNTAX");
    }
    PrintColor("[1] Tabla evaluada (aritmetica, funciones, SI/CONCAT/LARGO y errores)", "1;33");
    sheet.print(std::cout);

    PrintColor("[2] Persistencia en archivo (SaveTo / LoadFrom -> demo_sheet.txt)", "1;33");
    WithDemoFile("demo_sheet.txt", [&] {
        SaveTo("demo_sheet.txt", sheet);
        SheetOf<> loaded;
        LoadFrom("demo_sheet.txt", loaded);
        std::ostringstream a, b;
        a << sheet;
        b << loaded;
        Check(a.str() == b.str() && loaded.value("B2") == 54 && loaded.text("C1") == "Total", name,
              "el archivo leido no coincide");
        PrintSerializedSheet(a.str(), std::cout);
    });

    PrintColor("[3] Validacion ante entrada invalida (no corrompe la hoja)", "1;33");
    std::istringstream invalid("[(A1,5),(nada,1)]");
    SheetOf<> target;
    target.set("A1", "7");
    invalid >> target;
    Check(invalid.fail() && target.value("A1") == 7, name, "un texto invalido debio rechazarse");
    std::cout << "  Entrada \"[(A1,5),(nada,1)]\" rechazada -> A1 conserva " << target.text("A1") << '\n';

    PrintColor("[4] Concurrencia: 4 hilos escribiendo 200 celdas c/u + lectura simultanea", "1;33");
    std::vector<std::thread> threads;
    for (Int t = 0; t < 4; ++t)
        threads.emplace_back([&, t] {
            for (Int i = 1; i <= 200; ++i) {
                sheet.set("E" + std::to_string(t * 200 + i), "1");
                sheet.value("B3");
            }
        });
    for (auto& thread : threads) thread.join();
    sheet.set("F1", "=SUMA(E1:E800)");
    Check(sheet.value("F1") == 800, name, "escrituras concurrentes perdidas");
    std::cout << "  F1 = SUMA(E1:E800) -> " << sheet.text("F1") << '\n';
    PrintColor(name + ": formulas, archivo y 4 hilos OK", "1;32");
}

/** Imprime el texto serializado `[(celda,texto),...]` agrupado por columna y con color. */
void PrintSerializedSheet(const std::string& serialized, std::ostream& os = std::cout) {
    if (serialized.size() < 2 || serialized.front() != '[' || serialized.back() != ']') {
        os << serialized << '\n';
        return;
    }

    std::vector<std::string> entries;
    Bool in_quotes = false;
    Bool escaped = false;
    Int depth = 0;
    std::size_t start = 1;
    for (std::size_t i = 1; i + 1 < serialized.size(); ++i) {
        Char ch = serialized[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (in_quotes) {
            if (ch == '\\') escaped = true;
            else if (ch == '"') in_quotes = false;
            continue;
        }
        if (ch == '"') in_quotes = true;
        else if (ch == '(') ++depth;
        else if (ch == ')') --depth;
        else if (ch == ',' && depth == 0) {
            entries.push_back(serialized.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < serialized.size() - 1)
        entries.push_back(serialized.substr(start, serialized.size() - 1 - start));

    auto col_of = [](const std::string& entry) {
        std::string col;
        for (std::size_t i = 1; i < entry.size() && std::isalpha(static_cast<Byte>(entry[i])); ++i)
            col += entry[i];
        return col;
    };

    visual::Painter paint(os);
    auto print_entry = [&](const std::string& entry, Bool trailing_comma) {
        std::size_t comma = entry.find(',');
        if (comma == std::string::npos || entry.front() != '(' || entry.back() != ')') {
            os << entry << (trailing_comma ? "," : "");
            return;
        }
        std::string cell = entry.substr(1, comma - 1);
        std::string val = entry.substr(comma + 1, entry.size() - comma - 2);
        Bool is_formula = (!val.empty() && val.front() == '=') || (val.size() >= 2 && val[0] == '"' && val[1] == '=');
        os << paint("2", "(") << paint("1;36", cell) << paint("2", ",")
           << (is_formula ? paint("0;32", val) : val) << paint("2", trailing_comma ? ")," : ")");
    };

    os << paint("2", "[") << '\n';
    std::size_t i = 0;
    while (i < entries.size()) {
        std::string col = col_of(entries[i]);
        std::size_t j = i + 1;
        while (j < entries.size() && col_of(entries[j]) == col) ++j;

        std::size_t line_len = 2;
        for (std::size_t k = i; k < j; ++k) {
            line_len += entries[k].size();
            if (k + 1 < entries.size()) ++line_len;
            if (k + 1 < j) ++line_len;
        }

        if (line_len <= 78) {
            os << "  ";
            for (std::size_t k = i; k < j; ++k) {
                if (k > i) os << ' ';
                print_entry(entries[k], k + 1 < entries.size());
            }
            os << '\n';
        } else {
            for (std::size_t k = i; k < j; ++k) {
                os << (k == i ? "  " : "    ");
                print_entry(entries[k], k + 1 < entries.size());
                os << '\n';
            }
        }
        i = j;
    }
    os << paint("2", "]") << '\n';
}

void DemoSheetStepByStep() {
    PrintColor("DemoSheet paso a paso");
    const std::string name = "Sheet paso a paso";
    SheetOf<> sheet;
    auto step = [&](const std::string& title) {
        PrintColor(title, "1;33");
        sheet.print(std::cout);
    };

    sheet.set("A1", "10");
    sheet.set("A2", "20");
    sheet.set("A3", "30");
    sheet.set("C1", "Notas");
    step("set A1=10, A2=20, A3=30, C1=Notas: numeros y texto se guardan tal cual en la HashTable");

    sheet.set("B1", "=(A1+A2)*2");
    step("B1 = (A1+A2)*2: el parser resuelve primero el parentesis -> (10+20)*2");
    Check(sheet.value("B1") == 60, name, "parentesis");

    auto show = [](const std::string& title, const SheetOf<>::Formula& tree) {
        PrintColor(title, "1;33");
        tree.print_tree(std::cout);
        std::cout << "PostOrder:";
        for (const auto& node : tree.PostOrder()) std::cout << ' ' << node.getValue();
        std::cout << '\n';
    };
    show("AST de B1 = (A1+A2)*2: el parentesis deja el + debajo del *; no genera nodo", sheet.formula("B1"));
    show("AST de A1+A2*2 sin parentesis: ahora el * queda debajo del +", SheetOf<>::Formula("A1+A2*2"));
    PrintColor("Evaluar B1 recorriendo el AST en PostOrder con una pila:", "1;33");
    std::cout << "A1 -> [10]   A2 -> [10, 20]   + -> [30]   2 -> [30, 2]   * -> [60]\n";

    sheet.set("B2", "=SUMA(A1:A3)");
    sheet.set("B3", "=PROM(A1:A3) + MAX(A1, A2) - MIN(A1:A3)");
    step("B2 = SUMA(A1:A3), B3 = PROM(A1:A3) + MAX(A1, A2) - MIN(A1:A3): un rango aporta cada celda numerica");
    Check(sheet.value("B2") == 60 && sheet.value("B3") == 30, name, "funciones con rango");
    show("AST de B3: un rango es una hoja; los argumentos se encadenan con nodos ','", sheet.formula("B3"));

    sheet.set("A1", "40");
    step("A1 = 40: no se guardan resultados, asi que B1, B2 y B3 se recalculan en cascada");
    Check(sheet.value("B1") == 120 && sheet.value("B2") == 90, name, "cascada");

    sheet.set("D1", "=D2");
    sheet.set("D2", "=D1+1");
    sheet.set("D3", "=A1/(A2-20)");
    step("D1 = D2, D2 = D1+1 -> #CICLO;  D3 = A1/(A2-20) -> #DIV/0");
    Check(sheet.text("D1") == "#CICLO" && sheet.text("D3") == "#DIV/0", name, "errores");

    SheetOf<> grades;
    auto gstep = [&](const std::string& title) {
        PrintColor(title, "1;33");
        grades.print(std::cout);
    };
    auto gstatus = [&](const std::string& student_cell, const std::string& prom_cell,
                       const std::string& state_cell, const std::string& summary_cell) {
        std::string st = grades.text(state_cell);
        std::cout << "  " << grades.text(student_cell) << " (prom=" << grades.text(prom_cell) << ") -> ";
        PrintColor(st + " | " + grades.text(summary_cell), st == "Aprobado" ? "1;32" : "1;31");
    };

    grades.set("A1", "Alumno");
    grades.set("B1", "Nota1");
    grades.set("C1", "Nota2");
    grades.set("D1", "Prom");
    grades.set("E1", "Estado");
    grades.set("F1", "Resumen");

    grades.set("A2", "Ana");
    grades.set("B2", "14");
    grades.set("C2", "16.5");
    grades.set("D2", "=PROM(B2:C2)");
    grades.set("E2", "=SI(D2>=11, \"Aprobado\", \"Desaprobado\")");
    grades.set("F2", "=CONCAT(A2, \": \", E2)");

    grades.set("A3", "Luis");
    grades.set("B3", "8");
    grades.set("C3", "11");
    grades.set("D3", "=PROM(B3:C3)");
    grades.set("E3", "=SI(D3>=11, \"Aprobado\", \"Desaprobado\")");
    grades.set("F3", "=CONCAT(A3, \": \", E3, \" (\", LARGO(E3), \")\")");

    gstep("Registro de notas: texto, decimales, PROM, SI y CONCAT");
    gstatus("A2", "D2", "E2", "F2");
    gstatus("A3", "D3", "E3", "F3");
    Check(grades.text("E2") == "Aprobado" && grades.text("E3") == "Desaprobado" && grades.text("F2") == "Ana: Aprobado",
          name, "registro inicial con SI y CONCAT");

    grades.set("C3", "16");
    gstep("Cambio C3 = 16: Luis sube de promedio y pasa a Aprobado en cascada");
    gstatus("A3", "D3", "E3", "F3");
    Check(grades.value("D3") == 12 && grades.text("E3") == "Aprobado", name, "cambio de Desaprobado a Aprobado");

    grades.remove("C3");
    grades.set("G1", "=B2 + 1");
    grades.remove("B2");
    gstep("remove(C3) y remove(B2): en rango PROM ignora vacias; en ref directa G1=B2+1 da #REF");
    gstatus("A2", "D2", "E2", "F2");
    gstatus("A3", "D3", "E3", "F3");
    Check(grades.value("D3") == 8 && grades.text("E3") == "Desaprobado" && grades.text("G1") == "#REF", name,
          "remove en rango y en referencia directa");

    grades.set("G2", "=G3");
    grades.set("G3", "=G2+1");
    grades.set("G4", "=A1+1");
    grades.set("G5", "=NOEXISTE(1)");
    gstep("Errores: G1=#REF, G2/G3=#CICLO, G4=A1+1 (#VALUE por sumar texto), G5=#NAME");
    Check(grades.text("G2") == "#CICLO" && grades.text("G4") == "#VALUE" && grades.text("G5") == "#NAME", name,
          "errores variados");

    PrintColor("operator<<: la HashTable escribe [(celda,texto),...]; las formulas con , o ) van entre comillas", "1;33");
    std::stringstream saved;
    saved << grades;
    PrintSerializedSheet(saved.str());

    SheetOf<> loaded;
    saved >> loaded;
    PrintColor("operator>>: se lee en otra hoja y se recalcula igual", "1;33");
    loaded.print(std::cout);
    Check(saved && loaded.text("E2") == grades.text("E2") && loaded.text("G2") == "#CICLO", name, "lectura");
}
