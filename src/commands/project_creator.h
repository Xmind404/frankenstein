#pragma once

#include <filesystem>

class ProjectCreator {
  public:
    bool create(const std::filesystem::path& project_path);
};