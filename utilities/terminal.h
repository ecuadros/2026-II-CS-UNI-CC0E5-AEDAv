#ifndef __TERMINAL_H__
#define __TERMINAL_H__
#include <iostream>
#include <string>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "../types.h"

class Terminal {
    termios m_saved;

    static bool pending() {
        pollfd input{STDIN_FILENO, POLLIN, 0};
        return poll(&input, 1, 25) > 0;
    }
    static TX read_byte() {
        unsigned char byte;
        TX key = Closed;
        if (::read(STDIN_FILENO, &byte, 1) == 1)
            key = byte;
        return key;
    }

public:
    enum Key : TX { Unknown, Tab = 9, Enter = 10, Quit = 17, Save = 19, Escape = 27, Backspace = 127, Up = 256, Down, Left, Right, Delete, Closed };

    Terminal() {
        tcgetattr(STDIN_FILENO, &m_saved);
        termios raw = m_saved;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
        raw.c_iflag &= ~IXON;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        std::cout << "\033[?1049h\033[?25l\033[?7l";
    }
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    ~Terminal() {
        std::cout << "\033[?7h\033[?25h\033[?1049l" << std::flush;
        tcsetattr(STDIN_FILENO, TCSANOW, &m_saved);
    }

    static bool available() { return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO); }

    static winsize size() {
        winsize size{};
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        return size;
    }

    static void clear() { std::cout << "\033[H\033[2J"; }

    static TX read_key() {
        TX key = read_byte();
        if (key == '\b')
            return Backspace;
        if (key != Escape || !pending())
            return key;
        std::string sequence;
        do
            sequence += static_cast<char>(read_byte());
        while ((sequence.size() == 1 || sequence.back() < '@') && pending());
        if (sequence == "[3~")
            return Delete;
        if (sequence.size() != 2 || (sequence[0] != '[' && sequence[0] != 'O'))
            return Unknown;
        switch (sequence[1]) {
        case 'A': return Up;
        case 'B': return Down;
        case 'C': return Right;
        case 'D': return Left;
        default:  return Unknown;
        }
    }
};

#endif // __TERMINAL_H__
