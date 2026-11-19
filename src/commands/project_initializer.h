#pragma once

#include <filesystem>

#include "../configuration/project_configuration.h"

class ProjectInitializer {
  public:
    enum class Mode {
        NewProject,
        CurrentDirectory
    };

    bool initialize(
        const std::filesystem::path& project_path,
        const configuration::ProjectConfiguration& configuration,
        Mode mode
    );
};