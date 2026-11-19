#include "diagnostics_command.h"
#include "../cli/console.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifdef FRANKENSTEIN_HAS_CPR
#include <cpr/cpr.h>
#endif

int DiagnosticsCommand::execute(const CLI& cli) {
    if (action == Action::Doctor) {
        std::cout << "Frankenstein environment check\n";
        const int cmake = std::system("cmake --version");
#ifdef _WIN32
        const int compiler = std::system("where cl");
#else
        const int compiler = std::system("c++ --version");
#endif
        if (cmake != 0) Console::error("CMake was not found.");
        else Console::success("CMake is available.");
        if (compiler != 0) Console::warning("No default C++ compiler was found.");
        else Console::success("A C++ compiler is available.");
        return cmake == 0 && compiler == 0 ? 0 : 1;
    }
    if (action == Action::Info) {
        std::ifstream manifest("Frankenstein.toml");
        if (!manifest) { Console::error("Frankenstein.toml not found in the current directory."); return 1; }
        std::cout << std::string(std::istreambuf_iterator<char>(manifest), {});
        return 0;
    }
    if (cli.getArguments().empty()) { Console::error("Usage: frankenstein search <query>"); return 1; }
#ifndef FRANKENSTEIN_HAS_CPR
    Console::error("This Frankenstein build has no CPR support.");
    return 1;
#else
    std::string query;
    for (const auto& part : cli.getArguments()) { if (!query.empty()) query += "+"; query += part; }
    for (char& c : query) if (c == ' ') c = '+';
    const auto response = cpr::Get(cpr::Url{"https://api.github.com/search/repositories?q=" + query},
        cpr::Header{{"Accept", "application/vnd.github+json"}, {"User-Agent", "frankenstein"}},
        cpr::VerifySsl{true}, cpr::Timeout{20000});
    if (response.error || response.status_code != 200) {
        Console::error("GitHub search failed (HTTP " + std::to_string(response.status_code) + ").");
        return 1;
    }
    const std::string key = "\"full_name\": \"";
    std::size_t pos = 0;
    int count = 0;
    while ((pos = response.text.find(key, pos)) != std::string::npos && count < 10) {
        pos += key.size();
        const auto end = response.text.find('"', pos);
        if (end == std::string::npos) break;
        Console::print(response.text.substr(pos, end - pos));
        pos = end + 1;
        ++count;
    }
    if (count == 0) Console::print("No repositories found.");
    return 0;
#endif
}
