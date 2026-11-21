#include "update_command.h"

#include "../cli/console.h"
#include "../configuration/project_dependencies.h"
#include "../configuration/project_manifest.h"
#include "../configuration/project_root.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

}

int UpdateCommand::execute(const CLI& cli) {
  if (cli.getArguments().empty()) {
    Console::error("Usage: frankenstein update <owner/repository>");
    return 1;
  }

  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  const std::string repository = cli.getArguments()[0];
  const fs::path manifestPath = root / "Frankenstein.toml";
  configuration::Manifest manifest = configuration::readManifest(manifestPath);

  auto iterator = std::find_if(
    manifest.dependencies.begin(),
    manifest.dependencies.end(),
    [&](const configuration::Dependency& dependency) { return dependency.repository == repository; }
  );

  if (iterator == manifest.dependencies.end()) {
    Console::error("Dependency is not tracked: " + repository);
    return 1;
  }

  if (!configuration::dependencyExistsOnDisk(root, repository)) {
    Console::error("Dependency is not installed in external/: " + repository);
    return 1;
  }

  Console::print("Removing external/" + iterator->name + "...");
  if (!configuration::removeDependencyFiles(root, repository)) {
    Console::error("Could not remove external files for " + repository + ".");
    return 1;
  }

  Console::print("Downloading " + repository + " (" + iterator->branch + ")...");
  const configuration::DependencyInstallResult install = configuration::downloadDependency(
    root,
    repository,
    iterator->branch,
    std::string{},
    std::string{}
  );
  if (!install.success) {
    Console::error(install.error);
    return 1;
  }

  Console::success("Updated " + repository + " in external/" + iterator->name + ".");
  Console::print("Run 'frankenstein sync' to regenerate Frankenstein.lock.");
  return 0;
}