#include "project_structure.h"

#include <array>
#include <system_error>

bool ProjectStructure::create(const std::filesystem::path& project_path) {
    const std::array<std::filesystem::path, 3> directories = {
        "include",
        "src",
        "tests"
    };

    for (const std::filesystem::path& directory : directories) {
        std::error_code error;

        if (!std::filesystem::create_directory(project_path / directory, error) || error) {
            return false;
        }
    }

    return true;
}