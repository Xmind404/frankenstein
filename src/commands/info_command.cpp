#include "info_command.h"

#include "../cli/console.h"
#include "../configuration/project_root.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int InfoCommand::execute(const CLI& cli) {
  (void)cli;
  const std::filesystem::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  std::ifstream manifest(root / "Frankenstein.toml");
  if (!manifest) {
    Console::error("Could not open Frankenstein.toml.");
    return 1;
  }

  std::cout << std::string(std::istreambuf_iterator<char>(manifest), {});
  return 0;
}