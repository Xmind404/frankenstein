#include "doctor_command.h"

#include "../cli/console.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

bool commandExists(const std::string& command) {
#ifdef _WIN32
  const std::string invocation = "where " + command + " >NUL 2>&1";
#else
  const std::string invocation = "command -v " + command + " >/dev/null 2>&1";
#endif
  return std::system(invocation.c_str()) == 0;
}

}

int DoctorCommand::execute(const CLI& cli) {
  (void)cli;
  bool healthy = true;
  const std::vector<std::string> required = {"cmake", "ctest"};
  for (const std::string& tool : required) {
    if (commandExists(tool)) Console::success(tool + " is available.");
    else {
      Console::error(tool + " was not found on PATH.");
      healthy = false;
    }
  }

#ifdef _WIN32
  const bool hasCompiler = commandExists("cl") || commandExists("clang++") || commandExists("g++");
#else
  const bool hasCompiler = commandExists("cc") || commandExists("gcc") || commandExists("clang") ||
                           commandExists("c++") || commandExists("g++") || commandExists("clang++");
#endif
  if (hasCompiler) Console::success("A C/C++ compiler is available.");
  else {
    Console::error("No C/C++ compiler was found on PATH.");
    healthy = false;
  }

#ifndef FRANKENSTEIN_HAS_CPR
  Console::warning("This build was compiled without CPR support. GitHub search and remote dependency downloads are unavailable.");
#else
  Console::success("CPR support is enabled. GitHub search and remote dependency downloads are available.");
#endif

  if (healthy) Console::success("The required local build tools are available.");
  else Console::warning("Install the missing tools and make sure they are available on PATH.");
  return healthy ? 0 : 1;
}
