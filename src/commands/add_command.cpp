#include "add_command.h"

#include "../cli/console.h"
#include "../cli/prompt.h"
#include "../configuration/project_dependencies.h"
#include "../configuration/project_manifest.h"
#include "../configuration/project_root.h"
#include "../configuration/project_sources.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

bool validTargetName(const std::string& target) {
  if (target.empty()) return false;
  std::size_t start = 0;
  while (start < target.size()) {
    const std::size_t separator = target.find("::", start);
    const std::string component = target.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
    if (component.empty() || !std::all_of(component.begin(), component.end(), [](unsigned char c) {
          return std::isalnum(c) || c == '_' || c == '.' || c == '+' || c == '-';
        })) return false;
    if (separator == std::string::npos) return true;
    start = separator + 2;
    if (start == target.size()) return false;
  }
  return false;
}

bool validBranchName(const std::string& branch) {
  if (branch.empty() || branch.front() == '/' || branch.back() == '/' ||
      branch.find("//") != std::string::npos || branch.find("..") != std::string::npos ||
      branch.find("@{") != std::string::npos || branch.back() == '.') return false;
  for (unsigned char c : branch) {
    if (c <= 0x20 || c == 0x7f || c == '~' || c == '^' || c == ':' || c == '?' ||
        c == '*' || c == '[' || c == '\\') return false;
  }
  std::istringstream parts(branch);
  std::string part;
  while (std::getline(parts, part, '/')) {
    if (part.empty() || part.front() == '.' ||
        (part.size() >= 5 && part.compare(part.size() - 5, 5, ".lock") == 0)) return false;
  }
  return true;
}

std::string chooseTarget(const std::string& repository, const std::vector<configuration::TargetInfo>& targets) {
  if (targets.empty()) {
    Console::warning("No CMake library target was detected for " + repository + ".");
    const std::string manual = prompt::input("CMake target name:", "");
    return manual;
  }
  if (targets.size() == 1) {
    Console::print("Detected CMake target: " + targets.front().name);
    return targets.front().name;
  }
  std::vector<std::string> options;
  for (const configuration::TargetInfo& target : targets) {
    options.push_back(target.name + "  [" + target.type + "]");
  }
  options.push_back("Enter target name manually");
  const std::size_t index = prompt::select("Select CMake target for " + repository, options, 0);
  if (index < targets.size()) {
    Console::print("Selected CMake target: " + targets[index].name);
    return targets[index].name;
  }
  const std::string manual = prompt::input("CMake target name:", "");
  return manual;
}

}

int AddCommand::execute(const CLI& cli) {
  if (cli.getArguments().empty()) {
    Console::error(
      "Usage: frankenstein add <owner/repository[:branch]>\n"
      "       frankenstein add <src/file.cpp>"
    );
    return 1;
  }

  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  const std::string argument = cli.getArguments()[0];
  if (configuration::isSourceFileArgument(argument)) {
    return addSource(cli, argument);
  }
  return addDependency(cli, argument);
}

int AddCommand::addSource(const CLI& cli, const std::string& argument) {
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  if (cli.hasFlag("--rev") || cli.hasFlag("--sha256") || cli.hasFlag("--target")) {
    Console::error("--rev, --sha256 and --target are only valid for repository dependencies.");
    return 1;
  }

  const std::string normalized = configuration::normalizeSourcePath(root, argument);
  if (normalized.empty()) {
    Console::error("Source path must stay inside the project directory: " + argument);
    return 1;
  }

  std::error_code error;
  if (!fs::is_regular_file(root / normalized, error) || error) {
    Console::error("Source file does not exist: " + normalized);
    return 1;
  }

  const fs::path manifestPath = root / "Frankenstein.toml";
  configuration::Manifest manifest = configuration::readManifest(manifestPath);
  const bool exists = std::find(manifest.sources.begin(), manifest.sources.end(), normalized) != manifest.sources.end();
  if (exists) {
    Console::warning("Source already tracked: " + normalized);
    return 0;
  }

  manifest.sources.push_back(normalized);
  std::sort(manifest.sources.begin(), manifest.sources.end());

  if (!configuration::writeManifest(manifestPath, manifest)) {
    Console::error("Could not update Frankenstein.toml.");
    return 1;
  }

  Console::success("Added source: " + normalized);
  Console::print("Run 'frankenstein sync' to regenerate CMakeLists.txt.");
  return 0;
}

int AddCommand::addDependency(const CLI& cli, const std::string& argument) {
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) return 1;

  std::string repository = argument;
  std::string branch = "main";
  const std::size_t colon = repository.find(':');
  if (colon != std::string::npos) {
    branch = repository.substr(colon + 1);
    repository = repository.substr(0, colon);
  }

  const std::size_t slash = repository.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 >= repository.size() ||
      repository.find('/', slash + 1) != std::string::npos || branch.empty() ||
      repository.find_first_of("\\\\ \t\r\n") != std::string::npos ||
      branch.find_first_of("\\\\ \t\r\n") != std::string::npos ||
      repository.substr(0, slash) == "." || repository.substr(0, slash) == ".." ||
      repository.substr(slash + 1) == "." || repository.substr(slash + 1) == ".." ||
      repository.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-/") != std::string::npos ||
      !validBranchName(branch)) {
    Console::error("Expected owner/repository[:branch] with non-empty owner, repository and branch.");
    return 1;
  }

  const fs::path manifestPath = root / "Frankenstein.toml";
  configuration::Manifest manifest = configuration::readManifest(manifestPath);
  const std::string name = configuration::dependencyNameFromRepository(repository);

  const auto declared = std::find_if(
    manifest.dependencies.begin(), manifest.dependencies.end(),
    [&](const configuration::Dependency& dependency) {
      return dependency.repository == repository || dependency.name == name ||
             configuration::dependencyNameFromRepository(dependency.repository) == name;
    }
  );
  if (declared != manifest.dependencies.end()) {
    Console::print("Dependency already exists in Frankenstein.toml: " + declared->repository +
                   ". It was not added again.");
    return 0;
  }

  const fs::path externalDirectory = root / "external";
  const fs::path destination = externalDirectory / name;
  std::error_code error;
  if (fs::exists(destination, error)) {
    if (error) {
      Console::error("Could not inspect external/" + name + ".");
      return 1;
    }
    Console::print("Dependency files already exist: external/" + name +
                   ". Nothing was downloaded or overwritten.");
    return 0;
  }
  if (error) {
    Console::error("Could not inspect external/ directory.");
    return 1;
  }

  const std::string explicitCommit = cli.hasFlag("--rev") ? cli.getFlagValue("--rev") : std::string{};
  const std::string explicitSha = cli.hasFlag("--sha256") ? cli.getFlagValue("--sha256") : std::string{};
  if ((cli.hasFlag("--rev") && explicitCommit.empty()) ||
      (cli.hasFlag("--sha256") && explicitSha.empty())) {
    Console::error("--rev and --sha256 require non-empty values.");
    return 1;
  }

  Console::print("Downloading " + repository + " (" + branch + ")...");
  const configuration::DependencyInstallResult install = configuration::downloadDependency(
    root, repository, branch, explicitCommit, explicitSha
  );
  if (!install.success) {
    Console::error(install.error);
    return 1;
  }

  const fs::path dependencyCmake = destination / "CMakeLists.txt";
  error.clear();
  if (!fs::is_regular_file(dependencyCmake, error) || error) {
    configuration::removeDependencyFiles(root, repository);
    Console::error("Dependency does not contain a usable CMakeLists.txt; it was not registered.");
    return 1;
  }

  std::string target;
  const std::vector<configuration::TargetInfo> targets = configuration::detectTargets(root, name);
  if (cli.hasFlag("--target") && !cli.getFlagValue("--target").empty()) {
    target = cli.getFlagValue("--target");
    const bool detected = std::any_of(targets.begin(), targets.end(), [&](const configuration::TargetInfo& item) {
      return item.name == target;
    });
    if (!detected && !targets.empty()) {
      Console::warning("Target '" + target + "' was not among the detected dependency targets; using it as requested.");
    }
    Console::print("Using CMake target: " + target);
  } else {
    Console::print("Detecting CMake targets for " + repository + "...");
    target = chooseTarget(repository, targets);
    if (target.empty()) {
      configuration::removeDependencyFiles(root, repository);
      Console::error("No CMake target selected. The dependency was not registered.");
      return 1;
    }
  }

  if (!validTargetName(target)) {
    configuration::removeDependencyFiles(root, repository);
    Console::error("Invalid CMake target name. Use letters, digits, '_', '.', '+', '-', or '::' namespace separators.");
    return 1;
  }

  configuration::Dependency dependency;
  dependency.name = name;
  dependency.repository = repository;
  dependency.branch = branch;
  dependency.target = target;
  manifest.dependencies.push_back(dependency);
  std::sort(manifest.dependencies.begin(), manifest.dependencies.end(),
    [](const configuration::Dependency& left, const configuration::Dependency& right) {
      return left.repository < right.repository;
    });

  if (!configuration::writeManifest(manifestPath, manifest)) {
    configuration::removeDependencyFiles(root, repository);
    Console::error("Could not update Frankenstein.toml. The downloaded dependency was rolled back.");
    return 1;
  }

  Console::success("Added " + repository + " to external/" + name + " (target: " + target + ")");
  Console::print("Run 'frankenstein sync' to regenerate CMakeLists.txt and Frankenstein.lock.");
  return 0;
}

