#include "clean_command.h"

#include "../cli/console.h"
#include "../configuration/project_root.h"

#include <filesystem>

int CleanCommand::execute(const CLI& cli) {
  (void)cli;
  const std::filesystem::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  std::error_code error;
  std::filesystem::remove_all(root / "build", error);
  if (error) {
    Console::error("Could not remove build directory: " + error.message());
    return 1;
  }

  Console::success("Removed build/ directory.");
  return 0;
}