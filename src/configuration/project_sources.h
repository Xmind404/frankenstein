#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace configuration {

std::vector<std::string> readSources(const std::filesystem::path& manifest);
bool writeSources(const std::filesystem::path& manifest, const std::vector<std::string>& sources);
bool generateSourcesCmake(const std::filesystem::path& project_root, const std::vector<std::string>& sources);
bool isSourceFileArgument(const std::string& argument);
std::string normalizeSourcePath(const std::filesystem::path& project_root, const std::string& argument);

}