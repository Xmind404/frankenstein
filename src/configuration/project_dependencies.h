#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "project_manifest.h"

namespace configuration {

struct TargetInfo {
  std::string name;
  std::string type;
};

struct DependencyInstallResult {
  bool success = false;
  std::string commit;
  std::string sha256;
  std::string error;
};

bool isSourceFileArgument(const std::string& argument);
std::string normalizeSourcePath(const std::filesystem::path& root, const std::string& argument);
std::string dependencyNameFromRepository(const std::string& repository);
std::vector<TargetInfo> detectTargets(const std::filesystem::path& root, const std::string& dependencyName);
DependencyInstallResult downloadDependency(
  const std::filesystem::path& root,
  const std::string& repository,
  const std::string& branch,
  const std::string& explicitCommit,
  const std::string& explicitSha,
  bool allowReplaceUnmanaged = false
);
bool removeDependencyFiles(const std::filesystem::path& root, const std::string& repository);
bool dependencyExistsOnDisk(const std::filesystem::path& root, const std::string& repository);

}
