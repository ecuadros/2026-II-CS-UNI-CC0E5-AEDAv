#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "containers/sheet.h"
#include "utilities/sheeteditor.h"
#include "utilities/terminal.h"

using namespace std;

int main(int argc, char* argv[]) {
    string file = argc > 1 ? argv[1] : "hoja.txt";
    if (!Terminal::available()) {
        cerr << "El Excel interactivo necesita una terminal" << endl;
        return 1;
    }
    try {
        Sheet sheet(100, 26);
        if (filesystem::exists(file)) {
            ifstream in(file);
            if (!(in >> sheet) || sheet.rows() == 0 || sheet.cols() == 0) {
                cerr << file << " no es una hoja" << endl;
                return 1;
            }
        }
        SheetEditor(sheet, file).run();
    } catch (const exception& error) {
        cerr << file << ": " << error.what() << endl;
        return 1;
    }
    return 0;
}
