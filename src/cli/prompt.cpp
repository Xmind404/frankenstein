
#include "prompt.h"

#include "ascii.h"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {

    char getKey() {
#ifdef _WIN32
        const int key = _getch();

        if (key == 0 || key == 0xE0) {
            const int extended_key = _getch();

            if (extended_key == 72) {
                return 'A';
            }

            if (extended_key == 80) {
                return 'B';
            }

            return '\0';
        }

        return static_cast<char>(key);
#else
        termios old_terminal{};

        if (tcgetattr(STDIN_FILENO, &old_terminal) == -1) {
            return '\0';
        }

        termios new_terminal = old_terminal;

        new_terminal.c_lflag &= ~(ICANON | ECHO);
        new_terminal.c_cc[VMIN] = 1;
        new_terminal.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal) == -1) {
            return '\0';
        }

        char key = '\0';

        if (read(STDIN_FILENO, &key, 1) != 1) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal);
            return '\0';
        }

        if (key == '\033') {
            char sequence[2]{};

            if (read(STDIN_FILENO, &sequence[0], 1) == 1 &&
                read(STDIN_FILENO, &sequence[1], 1) == 1) {
                if (sequence[0] == '[') {
                    key = sequence[1];
                }
            }
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal);

        return key;
#endif
    }

}

namespace prompt {

    std::string input(const std::string& label, const std::string& default_value) {
        ascii::draw("◆", label, ascii::Color::Green, ascii::Color::White);

        if (!default_value.empty()) {
            ascii::draw("│", default_value, ascii::Color::Gray, ascii::Color::White);
        }

        std::cout << "└  ";
        std::cout.flush();

        std::string value;

        if (!std::getline(std::cin, value)) {
            return default_value;
        }

        return value.empty() ? default_value : value;
    }


    std::size_t select(
        const std::string& label,
        const std::vector<std::string>& options,
        std::size_t default_index
    ) {
        if (options.empty()) {
            return 0;
        }

        if (default_index >= options.size()) {
            default_index = 0;
        }

        std::size_t selected_index = default_index;
        bool first_render = true;

        while (true) {
            if (!first_render) {
                std::cout << "\033[" << options.size() + 3 << "A";
                std::cout << "\033[J";
            }

            first_render = false;

            ascii::draw("◆", label, ascii::Color::Green, ascii::Color::White);
            std::cout << "│\n";

            for (std::size_t i = 0; i < options.size(); ++i) {
                const bool selected = i == selected_index;

                ascii::draw(
                    selected ? "●" : "○",
                    options[i],
                    selected ? ascii::Color::Green : ascii::Color::Gray,
                    ascii::Color::White
                );
            }

            ascii::draw(
                "└  ↑/↓ to navigate • Enter: confirm",
                ascii::Color::Gray
            );

            std::cout.flush();

            const char key = getKey();

            if (key == 'A' && selected_index > 0) {
                --selected_index;
            } else if (key == 'B' && selected_index < options.size() - 1) {
                ++selected_index;
            } else if (key == '\n' || key == '\r') {
                std::cout << '\n';
                return selected_index;
            } else if (key == '\0') {
                return selected_index;
            }
        }
    }


}
