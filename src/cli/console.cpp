#include "console.h"

#include <iostream>

namespace { bool color_enabled = true; }

void Console::setColorEnabled(bool enabled) { color_enabled = enabled; }

void Console::print(const std::string& message) {
    std::cout << message << '\n';
}

void Console::success(const std::string& message) {
    std::cout << (color_enabled ? "\033[32m✓\033[0m " : "Success: ") << message << '\n';
}

void Console::warning(const std::string& message) {
    std::cout << (color_enabled ? "\033[33m!\033[0m " : "Warning: ") << message << '\n';
}

void Console::error(const std::string& message) {
    std::cerr << (color_enabled ? "\033[31merror:\033[0m " : "Error: ") << message << '\n';
}
