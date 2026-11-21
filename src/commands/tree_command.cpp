#include "tree_command.h"

#include "../cli/console.h"
#include "../configuration/project_manifest.h"
#include "../configuration/project_root.h"

#include <filesystem>
#include <string>

int TreeCommand::execute(const CLI& cli) {
  (void)cli;
  const std::filesystem::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  const configuration::Manifest manifest = configuration::readManifest(root / "Frankenstein.toml");

  Console::print(manifest.name + " v" + manifest.version);

  if (manifest.dependencies.empty()) {
    Console::print("└── (no dependencies)");
    return 0;
  }

  for (std::size_t index = 0; index < manifest.dependencies.size(); ++index) {
    const configuration::Dependency& dependency = manifest.dependencies[index];
    const bool last = index + 1 == manifest.dependencies.size();
    const std::string branch = dependency.branch.empty() ? "main" : dependency.branch;
    Console::print(std::string(last ? "└── " : "├── ") + dependency.name + " (" + dependency.repository + "@" + branch + ")");
    if (!dependency.target.empty()) {
      Console::print(std::string(last ? "    " : "│   ") + "└── target: " + dependency.target);
    }
  }

  return 0;
}