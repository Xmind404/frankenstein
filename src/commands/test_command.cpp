#include "test_command.h"

#include "../cli/console.h"
#include "../configuration/project_root.h"
#include "build_command.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

bool copyCompilationDatabase(const fs::path& root, const fs::path& buildDirectory) {
  std::error_code error;
  const fs::path source = buildDirectory / "compile_commands.json";
  if (!fs::is_regular_file(source, error) || error) return false;
  fs::copy_file(source, root / "compile_commands.json", fs::copy_options::overwrite_existing, error);
  return !error;
}
}

int TestCommand::execute(const CLI& cli) {
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) return 1;
  const std::string config = BuildCommand::configurationFromCLI(cli);
  const fs::path buildDirectory = root / "build" / config;
  std::error_code error;
  fs::create_directories(buildDirectory, error);
  if (error) {
    Console::error("Could not create build directory: " + error.message());
    return 1;
  }

  const std::string configure = "-S " + BuildCommand::quoteShellArgument(root.string()) +
      " -B " + BuildCommand::quoteShellArgument(buildDirectory.string()) +
      " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";
  if (BuildCommand::runCMake(configure) != 0) {
    Console::error("CMake configuration failed.");
    return 1;
  }
  if (!copyCompilationDatabase(root, buildDirectory)) {
    Console::warning("CMake did not produce compile_commands.json; clangd may use an older database.");
  }

  std::string build = "--build " + BuildCommand::quoteShellArgument(buildDirectory.string()) +
      " --config " + config;
  if (cli.hasFlag("--jobs") && !cli.getFlagValue("--jobs").empty()) {
    build += " --parallel " + BuildCommand::quoteShellArgument(cli.getFlagValue("--jobs"));
  }
  if (BuildCommand::runCMake(build) != 0) {
    Console::error("Build failed; tests were not started.");
    return 1;
  }

  const std::string testCommand = "ctest --test-dir " +
      BuildCommand::quoteShellArgument(buildDirectory.string()) + " --output-on-failure";
  const int testStatus = std::system(testCommand.c_str());
  if (testStatus != 0) {
    Console::error("One or more tests failed.");
    return testStatus;
  }
  Console::success("All tests passed (" + config + ").");
  return 0;
}
