#include "project_structure.h"

#include <array>
#include <system_error>

bool ProjectStructure::create(const std::filesystem::path& project_path) {
  const std::array<std::filesystem::path, 4> directories = {
    "include",
    "src",
    "tests",
    "external"
  };

  for (const std::filesystem::path& directory : directories) {
    std::error_code error;

    std::filesystem::create_directories(project_path / directory, error);
    if (error || !std::filesystem::is_directory(project_path / directory, error) || error) {
      return false;
    }
  }

  return true;
}
