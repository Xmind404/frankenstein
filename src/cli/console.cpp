#include "console.h"

#include <iostream>

void Console::print(const std::string& message) {
    std::cout << message << '\n';
}

void Console::success(const std::string& message) {
    std::cout << "Success: " << message << '\n';
}

void Console::warning(const std::string& message) {
    std::cout << "Warning: " << message << '\n';
}

void Console::error(const std::string& message) {
    std::cerr << "Error: " << message << '\n';
}