#include "build_command.h"

#include "../cli/console.h"
#include "../configuration/project_root.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

bool syncCompilationDatabase(const fs::path& root, const fs::path& buildDirectory) {
  const fs::path source = buildDirectory / "compile_commands.json";
  const fs::path destination = root / "compile_commands.json";
  std::error_code error;
  if (!fs::is_regular_file(source, error) || error) return false;
  fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
  return !error;
}

}

std::string BuildCommand::configurationFromCLI(const CLI& cli) {
  return cli.hasFlag("--release") ? "Release" : "Debug";
}

std::string BuildCommand::quoteShellArgument(const std::string& value) {
#ifdef _WIN32
  std::string out = "\"";
  for (char c : value) {
    if (c == '"') out += '\\';
    out += c;
  }
  return out + "\"";
#else
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  return out + "'";
#endif
}

int BuildCommand::runCMake(const std::string& arguments) {
  return std::system(("cmake " + arguments).c_str());
}

int BuildCommand::execute(const CLI& cli) {
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  std::error_code error;
  if (!fs::exists(root / "CMakeLists.txt", error) || error) {
    Console::error("CMakeLists.txt not found in the current directory.");
    return 1;
  }

  const std::string configuration = configurationFromCLI(cli);
  const fs::path buildDirectory = root / "build" / configuration;
  std::filesystem::create_directories(buildDirectory, error);
  if (error) {
    Console::error("Could not create build directory: " + error.message());
    return 1;
  }

  const int configureStatus = runCMake("-S " + quoteShellArgument(root.string()) +
      " -B " + quoteShellArgument(buildDirectory.string()) + " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON");
  if (configureStatus != 0) {
    Console::error("CMake configuration failed.");
    return configureStatus;
  }

  if (!syncCompilationDatabase(root, buildDirectory)) {
    Console::warning("Could not update compile_commands.json for clangd.");
  }

  std::string buildArguments = "--build " + quoteShellArgument(buildDirectory.string()) + " --config " + configuration;
  if (cli.hasFlag("--target") && !cli.getFlagValue("--target").empty()) {
    buildArguments += " --target " + quoteShellArgument(cli.getFlagValue("--target"));
  }
  if (cli.hasFlag("--jobs") && !cli.getFlagValue("--jobs").empty()) {
    buildArguments += " --parallel " + quoteShellArgument(cli.getFlagValue("--jobs"));
  }

  const int buildStatus = runCMake(buildArguments);
  if (buildStatus != 0) {
    Console::error("Build failed.");
    return buildStatus;
  }

  Console::success("Built " + configuration + " in build/" + configuration + ".");
  return 0;
}