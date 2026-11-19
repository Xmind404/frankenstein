#include "ascii.h"

#include <iostream>

namespace ascii {

    namespace {

        const char* getColorCode(Color color) {
            switch (color) {
                case Color::Red:     return "\033[31m";
                case Color::Green:   return "\033[32m";
                case Color::Yellow:  return "\033[33m";
                case Color::Blue:    return "\033[34m";
                case Color::Magenta: return "\033[35m";
                case Color::Cyan:    return "\033[36m";
                case Color::White:   return "\033[37m";
                case Color::Gray:    return "\033[90m";
                case Color::Default: return "\033[0m";
            }

            return "\033[0m";
        }

    }

    void draw(const std::string& content) {
        std::cout << content << '\n';
    }

    void draw(const std::string& content, Color color) {
        std::cout << getColorCode(color) << content << "\033[0m\n";
    }

    void draw(
        const std::string& symbol,
        const std::string& content,
        Color symbol_color,
        Color content_color
    ) {
        std::cout
            << getColorCode(symbol_color) << symbol << "\033[0m"
            << " "
            << getColorCode(content_color) << content << "\033[0m"
            << '\n';
    }

    void draw(const std::vector<std::string>& lines, Color color) {
        for (const std::string& line : lines) {
            std::cout << getColorCode(color) << line << "\033[0m\n";
        }
    }
}