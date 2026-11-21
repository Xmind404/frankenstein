#include "remove_command.h"

#include "../cli/console.h"
#include "../configuration/project_dependencies.h"
#include "../configuration/project_manifest.h"
#include "../configuration/project_root.h"
#include "../configuration/project_cmake_sync.h"
#include "../configuration/project_lock.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

}

int RemoveCommand::execute(const CLI& cli) {
  if (cli.getArguments().empty()) {
    Console::error(
      "Usage: frankenstein remove <owner/repository>\n"
      "       frankenstein remove <src/file.cpp>"
    );
    return 1;
  }

  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  const std::string argument = cli.getArguments()[0];
  if (configuration::isSourceFileArgument(argument)) {
    return removeSource(cli, argument);
  }
  return removeDependency(cli, argument);
}

int RemoveCommand::removeSource(const CLI& cli, const std::string& argument) {
  (void)cli;
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  const std::string normalized = configuration::normalizeSourcePath(root, argument);
  if (normalized.empty()) {
    Console::error("Source path must stay inside the project directory: " + argument);
    return 1;
  }

  const fs::path manifestPath = root / "Frankenstein.toml";
  configuration::Manifest manifest = configuration::readManifest(manifestPath);
  const std::size_t before = manifest.sources.size();
  manifest.sources.erase(
    std::remove(manifest.sources.begin(), manifest.sources.end(), normalized),
    manifest.sources.end()
  );

  if (manifest.sources.size() == before) {
    Console::warning("Source is not tracked: " + normalized);
    return 0;
  }

  if (!configuration::writeManifest(manifestPath, manifest)) {
    Console::error("Could not update Frankenstein.toml.");
    return 1;
  }

  Console::success("Removed source: " + normalized);
  Console::print("Run 'frankenstein sync' to regenerate CMakeLists.txt.");
  return 0;
}

int RemoveCommand::removeDependency(const CLI& cli, const std::string& argument) {
  (void)cli;
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) return 1;

  const fs::path manifestPath = root / "Frankenstein.toml";
  configuration::Manifest manifest = configuration::readManifest(manifestPath);
  const auto iterator = std::find_if(
    manifest.dependencies.begin(), manifest.dependencies.end(),
    [&](const configuration::Dependency& dependency) { return dependency.repository == argument; }
  );
  if (iterator == manifest.dependencies.end()) {
    Console::print("Dependency is not declared in Frankenstein.toml: " + argument +
                   ". No files were removed.");
    return 0;
  }

  const std::string dependencyName = configuration::dependencyNameFromRepository(iterator->repository);
  if (dependencyName.empty() || dependencyName == "." || dependencyName == ".." ||
      dependencyName.find_first_of("/\\") != std::string::npos) {
    Console::error("The dependency repository in Frankenstein.toml has an invalid directory name; refusing to remove files.");
    return 1;
  }
  const fs::path dependencyDirectory = root / "external" / dependencyName;
  const fs::path stagedDirectory = root / "external" / (".frankenstein-remove-" + dependencyName);
  std::error_code error;
  bool staged = false;
  const bool directoryExists = fs::exists(dependencyDirectory, error);
  if (error) {
    Console::error("Could not inspect external/" + dependencyName + ".");
    return 1;
  }

  if (directoryExists) {
    if (fs::exists(stagedDirectory, error) || error) {
      Console::error("A temporary removal directory already exists: " + stagedDirectory.string());
      return 1;
    }
    fs::rename(dependencyDirectory, stagedDirectory, error);
    if (error) {
      Console::error("Could not safely stage external/" + dependencyName + " for removal: " + error.message());
      return 1;
    }
    staged = true;
  }

  manifest.dependencies.erase(iterator);
  if (!configuration::writeManifest(manifestPath, manifest)) {
    if (staged) {
      std::error_code restoreError;
      fs::rename(stagedDirectory, dependencyDirectory, restoreError);
    }
    Console::error("Could not update Frankenstein.toml. The dependency directory was restored when possible.");
    return 1;
  }

  if (staged) {
    fs::remove_all(stagedDirectory, error);
    if (error) {
      Console::warning("The manifest entry was removed, but temporary files remain at " + stagedDirectory.string() + ".");
    }
  }

  const fs::path lockPath = root / "Frankenstein.lock";
  if (!configuration::removeLockEntry(lockPath, argument)) {
    Console::warning("The dependency was removed, but Frankenstein.lock could not be updated.");
  }

  if (!configuration::synchronizeDependencies(root, manifest.dependencies)) {
    Console::warning("The dependency was removed, but CMakeLists.txt could not be synchronized. Run 'frankenstein sync'.");
  }

  if (directoryExists) {
    Console::success("Removed dependency " + argument + " and its external/" + dependencyName + " directory.");
  } else {
    Console::success("Removed dependency " + argument + " from Frankenstein.toml; its external/ directory was already missing.");
  }
  Console::print("Run 'frankenstein sync' to regenerate the build files and compile_commands.json.");
  return 0;
}

