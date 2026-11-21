#include "sync_command.h"

#include "../cli/console.h"
#include "../configuration/project_cmake_sync.h"
#include "../configuration/project_dependencies.h"
#include "../configuration/project_lock.h"
#include "../configuration/project_manifest.h"
#include "../configuration/project_root.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
namespace fs = std::filesystem;

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
  std::string result = "\"";
  for (char c : value) {
    if (c == '"') result += '\\';
    result += c;
  }
  return result + "\"";
#else
  std::string result = "'";
  for (char c : value) {
    if (c == '\'') result += "'\\''";
    else result += c;
  }
  return result + "'";
#endif
}

bool isHex(const std::string& value, std::size_t length) {
  if (value.size() != length) return false;
  return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

bool validTargetName(const std::string& target) {
  if (target.empty()) return true;
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

struct InstalledMarker {
  std::string repository;
  std::string branch;
  std::string commit;
  std::string sha256;
};

bool readMarker(const fs::path& packageDirectory, InstalledMarker& marker) {
  std::ifstream input(packageDirectory / ".frankenstein-dependency");
  if (!input) return false;
  return static_cast<bool>(std::getline(input, marker.repository)) &&
         static_cast<bool>(std::getline(input, marker.branch)) &&
         static_cast<bool>(std::getline(input, marker.commit)) &&
         static_cast<bool>(std::getline(input, marker.sha256));
}

bool validRepository(const std::string& repository) {
  const std::size_t slash = repository.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 >= repository.size() ||
      repository.find('/', slash + 1) != std::string::npos) return false;
  const auto validPart = [](const std::string& part) {
    return !part.empty() && part != "." && part != ".." &&
        std::all_of(part.begin(), part.end(), [](unsigned char c) {
          return std::isalnum(c) || c == '-' || c == '_' || c == '.';
        });
  };
  return validPart(repository.substr(0, slash)) && validPart(repository.substr(slash + 1));
}

bool validateManifest(const fs::path& manifestPath,
                      const configuration::Manifest& manifest,
                      std::string& errorMessage) {
  if (manifest.name.empty()) {
    errorMessage = "[project].name is missing or empty.";
    return false;
  }
  if (manifest.version.empty()) {
    errorMessage = "[project].version is missing or empty.";
    return false;
  }
  if (manifest.standard.empty()) {
    errorMessage = "[project].standard is missing or empty.";
    return false;
  }

  std::ifstream input(manifestPath);
  if (!input) {
    errorMessage = "Could not read Frankenstein.toml.";
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  const std::regex dependencySection(R"(^\s*\[dependencies\.([A-Za-z0-9_-]+)\]\s*(?:#.*)?$)", std::regex::multiline);
  std::set<std::string> sectionNames;
  std::size_t sectionCount = 0;
  for (std::sregex_iterator it(content.begin(), content.end(), dependencySection), end; it != end; ++it) {
    ++sectionCount;
    const std::string sectionName = (*it)[1].str();
    if (!sectionNames.insert(sectionName).second) {
      errorMessage = "Duplicate dependency section [dependencies." + sectionName + "].";
      return false;
    }
  }
  if (sectionCount != manifest.dependencies.size()) {
    errorMessage = "Frankenstein.toml contains a dependency section that could not be parsed. No files were changed.";
    return false;
  }

  std::unordered_set<std::string> repositories;
  std::unordered_set<std::string> names;
  for (const configuration::Dependency& dependency : manifest.dependencies) {
    if (dependency.name.empty() || dependency.repository.empty() || !validBranchName(dependency.branch)) {
      errorMessage = "Every dependency requires a name, repository and branch.";
      return false;
    }
    if (!validRepository(dependency.repository)) {
      errorMessage = "Invalid repository in [dependencies." + dependency.name + "]: " + dependency.repository;
      return false;
    }
    const std::string repositoryName = configuration::dependencyNameFromRepository(dependency.repository);
    if (dependency.name != repositoryName) {
      errorMessage = "Dependency key '" + dependency.name + "' must match the repository directory name '" + repositoryName + "'.";
      return false;
    }
    if (!validTargetName(dependency.target)) {
      errorMessage = "Invalid CMake target name in [dependencies." + dependency.name + "]: " + dependency.target;
      return false;
    }
    if (!names.insert(dependency.name).second || !repositories.insert(dependency.repository).second) {
      errorMessage = "Duplicate dependency detected in Frankenstein.toml: " + dependency.repository;
      return false;
    }
  }
  return true;
}

const configuration::LockEntry* findLockEntry(const std::vector<configuration::LockEntry>& entries,
                                               const std::string& repository) {
  const auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
    return entry.repository == repository;
  });
  return it == entries.end() ? nullptr : &*it;
}

bool ensureDependencyInstalled(const fs::path& root,
                               const configuration::Dependency& dependency,
                               const std::vector<configuration::LockEntry>& oldLock,
                               configuration::LockEntry& resultingLock,
                               std::string& errorMessage) {
  const fs::path packageDirectory = root / "external" / dependency.name;
  std::error_code error;
  const bool exists = fs::exists(packageDirectory, error);
  if (error) {
    errorMessage = "Could not inspect external/" + dependency.name + ": " + error.message();
    return false;
  }

  const configuration::LockEntry* locked = findLockEntry(oldLock, dependency.repository);
  const bool lockMatchesBranch = locked && locked->branch == dependency.branch &&
      isHex(locked->commit, 40) && isHex(locked->sha256, 64);

  if (exists) {
    if (!fs::is_directory(packageDirectory, error) || error) {
      errorMessage = "external/" + dependency.name + " exists but is not a directory.";
      return false;
    }
    InstalledMarker marker;
    const bool hasValidMarker = readMarker(packageDirectory, marker);
    if (!hasValidMarker) {
      Console::print("Replacing untracked external/" + dependency.name + " because the manifest declares it as a managed dependency.");
    }
    if (hasValidMarker && marker.repository != dependency.repository) {
      Console::print("Replacing managed package " + marker.repository + " with " + dependency.repository + ".");
    }

    std::error_code cmakeError;
    const bool hasCmake = fs::is_regular_file(packageDirectory / "CMakeLists.txt", cmakeError) && !cmakeError;
    const bool markerMatchesRepository = hasValidMarker && marker.repository == dependency.repository;
    const bool markerValid = hasValidMarker && marker.branch == dependency.branch &&
        isHex(marker.commit, 40) && isHex(marker.sha256, 64);
    const bool lockAndMarkerMatch = !locked ||
        (lockMatchesBranch && marker.commit == locked->commit && marker.sha256 == locked->sha256);

    if (hasCmake && markerMatchesRepository && markerValid && lockAndMarkerMatch) {
      resultingLock = {dependency.repository, dependency.branch, marker.commit, marker.sha256};
      Console::success("Dependency is up to date: " + dependency.repository + " (" + marker.commit.substr(0, 8) + ")");
      return true;
    }
  }

  std::string pinnedCommit;
  std::string pinnedSha;
  if (lockMatchesBranch) {
    pinnedCommit = locked->commit;
    pinnedSha = locked->sha256;
  }

  Console::print("Installing " + dependency.repository + " (" + dependency.branch + ")...");
  const configuration::DependencyInstallResult install = configuration::downloadDependency(
      root, dependency.repository, dependency.branch, pinnedCommit, pinnedSha, true);
  if (!install.success) {
    errorMessage = "Could not install " + dependency.repository + ": " + install.error;
    return false;
  }

  std::error_code cmakeError;
  if (!fs::is_regular_file(packageDirectory / "CMakeLists.txt", cmakeError) || cmakeError) {
    errorMessage = "Installed dependency " + dependency.repository + " has no CMakeLists.txt.";
    return false;
  }
  resultingLock = {dependency.repository, dependency.branch, install.commit, install.sha256};
  Console::success("Installed " + dependency.repository + " (" + install.commit.substr(0, 8) + ")");
  return true;
}

bool removeOrphanPackages(const fs::path& root,
                          const std::unordered_set<std::string>& expectedNames,
                          std::string& errorMessage) {
  const fs::path externalDirectory = root / "external";
  std::error_code error;
  if (!fs::exists(externalDirectory, error)) return !error;
  if (error || !fs::is_directory(externalDirectory, error) || error) {
    errorMessage = "external/ exists but cannot be read as a directory.";
    return false;
  }

  for (const fs::directory_entry& entry : fs::directory_iterator(externalDirectory, error)) {
    if (error) break;
    const std::string name = entry.path().filename().string();
    std::error_code typeError;
    if (!entry.is_directory(typeError) || typeError || expectedNames.find(name) != expectedNames.end()) continue;
    fs::remove_all(entry.path(), error);
    if (error) {
      errorMessage = "Could not remove unlisted package directory external/" + name + ": " + error.message();
      return false;
    }
    Console::print("Removed unlisted package: external/" + name);
  }
  if (error) {
    errorMessage = "Could not enumerate external/: " + error.message();
    return false;
  }
  return true;
}

bool copyCompilationDatabase(const fs::path& root, const fs::path& buildDirectory, std::string& errorMessage) {
  const fs::path source = buildDirectory / "compile_commands.json";
  const fs::path destination = root / "compile_commands.json";
  std::error_code error;
  if (!fs::is_regular_file(source, error) || error) {
    errorMessage = "CMake did not produce build/Debug/compile_commands.json.";
    return false;
  }
  fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
  if (error) {
    errorMessage = "Could not copy compile_commands.json to the project root: " + error.message();
    return false;
  }
  return true;
}
}

int SyncCommand::execute(const CLI& cli) {
  (void)cli;
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) return 1;

  const fs::path cmakePath = root / "CMakeLists.txt";
  std::error_code error;
  if (!fs::is_regular_file(cmakePath, error) || error) {
    Console::error("CMakeLists.txt not found in the project root.");
    return 1;
  }

  const fs::path manifestPath = root / "Frankenstein.toml";
  const configuration::Manifest manifest = configuration::readManifest(manifestPath);
  std::string message;
  if (!validateManifest(manifestPath, manifest, message)) {
    Console::error("Invalid Frankenstein.toml: " + message);
    return 1;
  }

  std::unordered_set<std::string> expectedNames;
  for (const auto& dependency : manifest.dependencies) expectedNames.insert(dependency.name);

  const fs::path lockPath = root / "Frankenstein.lock";
  const std::vector<configuration::LockEntry> oldLock = configuration::readLock(lockPath);
  std::vector<configuration::LockEntry> newLock;
  newLock.reserve(manifest.dependencies.size());

  fs::create_directories(root / "external", error);
  if (error) {
    Console::error("Could not create external/: " + error.message());
    return 1;
  }

  for (const configuration::Dependency& dependency : manifest.dependencies) {
    configuration::LockEntry entry;
    if (!ensureDependencyInstalled(root, dependency, oldLock, entry, message)) {
      Console::error(message);
      return 1;
    }
    newLock.push_back(std::move(entry));
  }

  if (!configuration::synchronizeStandard(root, message)) {
    Console::error(message.empty() ? "Could not synchronize the project standard." : message);
    return 1;
  }
  if (!configuration::synchronizeSources(root, manifest.sources)) {
    Console::error("Could not synchronize source files in CMakeLists.txt.");
    return 1;
  }
  if (!configuration::synchronizeDependencies(root, manifest.dependencies)) {
    Console::error("Could not synchronize dependency targets in CMakeLists.txt.");
    return 1;
  }

  if (!removeOrphanPackages(root, expectedNames, message)) {
    Console::error(message);
    return 1;
  }

  if (!configuration::writeLock(lockPath, newLock)) {
    Console::error("Could not write Frankenstein.lock.");
    return 1;
  }

  const fs::path buildDirectory = root / "build" / "Debug";
  fs::create_directories(buildDirectory, error);
  if (error) {
    Console::error("Could not create build/Debug: " + error.message());
    return 1;
  }
  const std::string configureCommand = "cmake -S " + shellQuote(root.string()) +
      " -B " + shellQuote(buildDirectory.string()) + " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";
  Console::print("Configuring CMake...");
  if (std::system(configureCommand.c_str()) != 0) {
    Console::error("CMake configuration failed.");
    return 1;
  }

  if (!copyCompilationDatabase(root, buildDirectory, message)) {
    Console::error(message);
    return 1;
  }

  Console::success("Project synchronized successfully.");
  Console::print("CMakeLists.txt, external/, Frankenstein.lock and compile_commands.json are up to date.");
  Console::print(".clangd was left unchanged; it continues to use CompilationDatabase: .");
  return 0;
}
