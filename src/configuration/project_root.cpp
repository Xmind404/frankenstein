#include "project_root.h"

#include "../cli/console.h"

namespace configuration {

bool isProjectRoot(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(path / "Frankenstein.toml", error) && !error;
}

std::filesystem::path requireProjectRoot() {
  const std::filesystem::path root = std::filesystem::current_path();
  if (!isProjectRoot(root)) {
    Console::error(
      "Frankenstein.toml not found in the current directory.\n"
      "Run this command from the project root (the directory containing "
      "CMakeLists.txt and Frankenstein.toml)."
    );
    return {};
  }
  return root;
}

}