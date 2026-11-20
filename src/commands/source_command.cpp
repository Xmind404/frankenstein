#include "source_command.h"

#include "../cli/console.h"
#include "../configuration/project_sources.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

bool fileExists(const fs::path& path) {
  std::error_code error;
  return fs::is_regular_file(path, error) && !error;
}

}

int SourceCommand::execute(const CLI& cli) {
  if (cli.getArguments().empty()) {
    Console::error(action == Action::Add
      ? "Usage: frankenstein add src/file.cpp"
      : "Usage: frankenstein remove src/file.cpp");
    return 1;
  }
  return action == Action::Add
    ? addSource(cli.getArguments()[0], cli)
    : removeSource(cli.getArguments()[0]);
}

int SourceCommand::addSource(const std::string& argument, const CLI& cli) {
  namespace fs = std::filesystem;
  if (cli.hasFlag("--rev") || cli.hasFlag("--sha256") || cli.hasFlag("--branch")) {
    Console::error("--rev, --sha256 and --branch are only valid for repository dependencies.");
    return 1;
  }
  const fs::path root = fs::current_path();
  const fs::path manifest = root / "Frankenstein.toml";
  if (!fs::exists(manifest)) {
    Console::error("Frankenstein.toml not found in the current directory.");
    return 1;
  }
  const std::string normalized = configuration::normalizeSourcePath(root, argument);
  if (normalized.empty()) {
    Console::error("Source path must stay inside the project directory: " + argument);
    return 1;
  }
  if (!fileExists(root / normalized)) {
    Console::error("Source file does not exist: " + normalized);
    return 1;
  }
  auto sources = configuration::readSources(manifest);
  if (std::find(sources.begin(), sources.end(), normalized) != sources.end()) {
    Console::warning("Source already tracked: " + normalized);
    return 0;
  }
  sources.push_back(normalized);
  std::sort(sources.begin(), sources.end());
  if (!configuration::writeSources(manifest, sources)) {
    Console::error("Could not update Frankenstein.toml.");
    return 1;
  }
  Console::success("Added source: " + normalized);
  return 0;
}

int SourceCommand::removeSource(const std::string& argument) {
  namespace fs = std::filesystem;
  const fs::path root = fs::current_path();
  const fs::path manifest = root / "Frankenstein.toml";
  if (!fs::exists(manifest)) {
    Console::error("Frankenstein.toml not found in the current directory.");
    return 1;
  }
  const std::string normalized = configuration::normalizeSourcePath(root, argument);
  if (normalized.empty()) {
    Console::error("Source path must stay inside the project directory: " + argument);
    return 1;
  }
  auto sources = configuration::readSources(manifest);
  const auto before = sources.size();
  sources.erase(std::remove(sources.begin(), sources.end(), normalized), sources.end());
  if (sources.size() == before) {
    Console::warning("Source is not tracked: " + normalized);
    return 0;
  }
  if (!configuration::writeSources(manifest, sources)) {
    Console::error("Could not update Frankenstein.toml.");
    return 1;
  }
  Console::success("Removed source: " + normalized);
  return 0;
}