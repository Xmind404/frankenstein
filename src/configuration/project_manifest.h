#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace configuration {

struct Dependency {
  std::string name;
  std::string repository;
  std::string branch;
  std::string target;
};

struct Manifest {
  std::string name;
  std::string version;
  std::string standard;
  std::vector<std::string> sources;
  std::vector<Dependency> dependencies;
  std::vector<std::string> linkLibraries;
  std::vector<std::string> windowsLinkLibraries;
  std::vector<std::string> linuxLinkLibraries;
  std::vector<std::string> macosLinkLibraries;
};

Manifest readManifest(const std::filesystem::path& path);
bool writeManifest(const std::filesystem::path& path, const Manifest& manifest);
bool manifestExists(const std::filesystem::path& path);

std::vector<std::string> resolvedLinkLibraries(const Manifest& manifest);
std::vector<std::string> resolvedLinkLibraries(const Manifest& manifest,
                                               const std::string& platform);
std::string currentPlatformName();

}