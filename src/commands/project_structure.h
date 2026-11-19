#pragma once

#include <filesystem>

class ProjectStructure {
    public:
        bool create(const std::filesystem::path& project_path);
};