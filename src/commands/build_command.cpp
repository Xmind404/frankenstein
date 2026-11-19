#include "build_command.h"
#include "../cli/console.h"
#include "../configuration/project_standard_sync.h"
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
std::string quote(const std::string& value) {
#ifdef _WIN32
  std::string q = "\"";
  for (char c : value) { if (c == '"') q += '\\'; q += c; }
  return q + "\"";
#else
  std::string q = "'";
  for (char c : value) { if (c == '\'') q += "'\\''"; else q += c; }
  return q + "'";
#endif
}
int cmake(const std::string& arguments) { return std::system(("cmake " + arguments).c_str()); }
}

int BuildCommand::execute(const CLI& cli) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists("CMakeLists.txt", ec) || ec) { Console::error("CMakeLists.txt not found in the current directory."); return 1; }
  const bool release = cli.hasFlag("--release");
  const std::string config = release ? "Release" : "Debug";
  const fs::path build_dir = fs::path("build") / config;
  if (action == Action::Clean) {
    fs::remove_all("build", ec);
    if (ec) { Console::error("Could not remove build directory: " + ec.message()); return 1; }
    Console::success("Build directory cleaned."); return 0;
  }
  std::string standard_sync_message;
  if (!configuration::synchronizeProjectStandard(
    fs::current_path(),
    standard_sync_message
  )) {
    Console::error(standard_sync_message);
    return 1;
  }
  if (!standard_sync_message.empty()) {
    Console::print(standard_sync_message);
  }
  if (action == Action::Check) return cmake("-S . -B " + quote(build_dir.string()));
  int result = cmake("-S . -B " + quote(build_dir.string()));
  if (result != 0) { Console::error("CMake configuration failed."); return result; }
  std::string build_args = "--build " + quote(build_dir.string()) + " --config " + config;
  if (action == Action::Test) build_args = "--build " + quote(build_dir.string()) + " --target test --config " + config;
  if (cli.hasFlag("--target") && !cli.getFlagValue("--target").empty()) build_args += " --target " + quote(cli.getFlagValue("--target"));
  if (cli.hasFlag("--jobs") && !cli.getFlagValue("--jobs").empty()) build_args += " --parallel " + quote(cli.getFlagValue("--jobs"));
  return cmake(build_args);
}
