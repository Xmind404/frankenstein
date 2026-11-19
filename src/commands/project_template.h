#pragma once

#include <filesystem>

#include "../configuration/project_configuration.h"

class ProjectTemplate {
  public:
  bool create(
    const std::filesystem::path& project_path,
    const configuration::ProjectConfiguration& configuration
  );
};