#include "run_command.h"
#include "build_command.h"
#include "../cli/console.h"
#include <cstdlib>
#include <filesystem>
#include <string>

int RunCommand::execute(const CLI& cli) {
    BuildCommand build;
    int result = build.execute(cli);
    if (result != 0) return result;
    namespace fs = std::filesystem;
    const std::string name = fs::current_path().filename().string();
#ifdef _WIN32
    const std::string suffix = ".exe";
#else
    const std::string suffix;
#endif
    for (const auto& entry : fs::recursive_directory_iterator("build")) {
        if (entry.is_regular_file() && (entry.path().filename() == name || entry.path().filename() == name + suffix)) {
            std::string command = "\"" + entry.path().string() + "\"";
            for (const auto& argument : cli.getArguments()) {
#ifdef _WIN32
                command += " \"" + argument + "\"";
#else
                command += " '";
                for (char c : argument) { if (c == '\'') command += "'\\''"; else command += c; }
                command += "'";
#endif
            }
            return std::system(command.c_str());
        }
    }
    Console::error("Built executable was not found. Set the CMake target name to the project directory name.");
    return 1;
}
