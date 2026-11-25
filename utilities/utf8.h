#ifndef __UTF8_H__
#define __UTF8_H__
#include <algorithm>
#include <string>

inline bool IsUtf8Start(char c) { return (c & 0xC0) != 0x80; }

inline size_t Utf8Width(const std::string& text) { return std::count_if(text.begin(), text.end(), IsUtf8Start); }

inline size_t Utf8Offset(const std::string& text, size_t x) {
    size_t pos = 0;
    for (; pos < text.size() && x > 0; --x)
        do
            ++pos;
        while (pos < text.size() && !IsUtf8Start(text[pos]));
    return pos;
}

#endif // __UTF8_H__
