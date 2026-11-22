#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "project_manifest.h"

namespace configuration {

bool synchronizeStandard(const std::filesystem::path& root, std::string& message);
bool synchronizeSources(const std::filesystem::path& root, const std::vector<std::string>& sources);
bool synchronizeDependencies(const std::filesystem::path& root, const std::vector<Dependency>& dependencies);
bool synchronizeLinking(const std::filesystem::path& root,
                        const Manifest& manifest,
                        std::string& message);

}