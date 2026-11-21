#include "publish_command.h"

#include "../cli/console.h"
#include "../cli/prompt.h"
#include "../configuration/project_manifest.h"
#include "../configuration/project_root.h"
#include "build_command.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

bool copyCompilationDatabase(const fs::path& root, const fs::path& buildDirectory) {
  std::error_code error;
  const fs::path source = buildDirectory / "compile_commands.json";
  if (!fs::is_regular_file(source, error) || error) return false;
  fs::copy_file(source, root / "compile_commands.json", fs::copy_options::overwrite_existing, error);
  return !error;
}

bool executableCandidate(const fs::path& path, const std::string& targetName) {
  std::string filename = path.filename().string();
#ifdef _WIN32
  if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".exe") filename.resize(filename.size() - 4);
#endif
  return filename == targetName;
}

fs::path findExecutable(const fs::path& buildDirectory, const std::string& targetName) {
  std::error_code error;
  fs::path match;
  for (fs::recursive_directory_iterator it(buildDirectory, fs::directory_options::skip_permission_denied, error), end;
       it != end && !error; it.increment(error)) {
    if (!it->is_regular_file(error) || error) continue;
    if (executableCandidate(it->path(), targetName)) {
      if (match.empty()) match = it->path();
      else return {};
    }
  }
  return match;
}
}

int PublishCommand::execute(const CLI& cli) {
  (void)cli;
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) return 1;
  const configuration::Manifest manifest = configuration::readManifest(root / "Frankenstein.toml");
  if (manifest.name.empty()) {
    Console::error("Could not determine the project name from Frankenstein.toml.");
    return 1;
  }

  const fs::path buildDirectory = root / "build" / "Release";
  std::error_code error;
  fs::create_directories(buildDirectory, error);
  if (error) {
    Console::error("Could not create build/Release: " + error.message());
    return 1;
  }

  const std::string configure = "-S " + BuildCommand::quoteShellArgument(root.string()) +
      " -B " + BuildCommand::quoteShellArgument(buildDirectory.string()) +
      " -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";
  if (BuildCommand::runCMake(configure) != 0) {
    Console::error("CMake Release configuration failed.");
    return 1;
  }
  if (!copyCompilationDatabase(root, buildDirectory)) {
    Console::warning("CMake did not produce compile_commands.json; clangd may use an older database.");
  }

  const std::string build = "--build " + BuildCommand::quoteShellArgument(buildDirectory.string()) +
      " --config Release";
  if (BuildCommand::runCMake(build) != 0) {
    Console::error("Release build failed.");
    return 1;
  }

  const fs::path executable = findExecutable(buildDirectory, manifest.name);
  if (executable.empty()) {
    Console::error("Could not identify one unambiguous executable named '" + manifest.name + "' in build/Release. Check that the CMake executable target matches [project].name.");
    return 1;
  }

  std::string outputName = prompt::input("Published executable name:", executable.filename().string());
  if (outputName.empty()) outputName = executable.filename().string();
  if (fs::path(outputName).filename() != fs::path(outputName)) {
    Console::error("The published name must be a filename, not a path.");
    return 1;
  }
#ifdef _WIN32
  if (fs::path(outputName).extension().empty()) outputName += ".exe";
#endif

  const fs::path publishDirectory = root / "publish";
  fs::create_directories(publishDirectory, error);
  if (error) {
    Console::error("Could not create publish/: " + error.message());
    return 1;
  }
  const fs::path destination = publishDirectory / outputName;
  fs::copy_file(executable, destination, fs::copy_options::overwrite_existing, error);
  if (error) {
    Console::error("Could not publish executable: " + error.message());
    return 1;
  }
  Console::success("Published executable: " + destination.string());
  return 0;
}
