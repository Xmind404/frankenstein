#include "project_creator.h"

#include <system_error>

bool ProjectCreator::create(const std::filesystem::path& project_path) {
  std::error_code error;

  if (std::filesystem::exists(project_path, error)) {
    return false;
  }

  if (error) {
    return false;
  }

  return std::filesystem::create_directories(project_path, error) && !error;
}