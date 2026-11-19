#include "ascii.h"

#include <iostream>

namespace ascii {

    namespace {
        bool color_enabled = true;

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
                case Color::DarkGreen: return "\033[38;5;22m";
                case Color::BrightGreen: return "\033[38;5;46m";
                case Color::DarkBlue: return "\033[38;5;25m";
                case Color::BrightBlue: return "\033[38;5;39m";
                case Color::Purple: return "\033[35m";
                case Color::Orange: return "\033[38;5;208m";
                case Color::Pink: return "\033[38;5;205m";
                case Color::Default: return "\033[0m";
            }

            return "\033[0m";
        }

    }

    void draw(const std::string& content) {
        std::cout << content << '\n';
    }

    void draw(const std::string& content, Color color) {
        if (!color_enabled) { std::cout << content << '\n'; return; }
        std::cout << getColorCode(color) << content << "\033[0m\n";
    }

    void draw(
        const std::string& symbol,
        const std::string& content,
        Color symbol_color,
        Color content_color
    ) {
        if (!color_enabled) { std::cout << symbol << " " << content << '\n'; return; }
        std::cout
            << getColorCode(symbol_color) << symbol << "\033[0m"
            << " "
            << getColorCode(content_color) << content << "\033[0m"
            << '\n';
    }

    void draw(const std::vector<std::string>& lines, Color color) {
        for (const std::string& line : lines) {
            if (!color_enabled) std::cout << line << '\n';
            else std::cout << getColorCode(color) << line << "\033[0m\n";
        }
    }

    void setColorEnabled(bool enabled) { color_enabled = enabled; }

    void drawLogo() {
        const std::vector<std::string> lines = {
            R"(  _____                      __                           __         .__        )",
            R"(_/ ____\___________    ____ |  | __ ____   ____   _______/  |_  ____ |__| ____  )",
            R"(\   __\\_  __ \__  \  /    \|  |/ // __ \ /    \ /  ___/\   __\/ __ \|  |/    \ )",
            R"( |  |   |  | \// __ \|   |  \    <\  ___/|   |  \\___ \  |  | \  ___/|  |   |  \ )",
            R"( |__|   |__|  (____  /___|  /__|_  \___  >___|  /____  > |__|  \___  >__|___|  /)",
            R"(                   \/     \/     \/    \/     \/     \/            \/        \/ )"
        };
        const Color colors[] = {Color::DarkGreen, Color::Green, Color::BrightGreen, Color::Cyan, Color::DarkBlue, Color::BrightBlue};
        for (std::size_t i = 0; i < lines.size(); ++i) draw(lines[i], colors[i]);
    }
}
