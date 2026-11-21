#pragma once

#include <filesystem>

namespace configuration {

std::filesystem::path requireProjectRoot();
bool isProjectRoot(const std::filesystem::path& path);

}