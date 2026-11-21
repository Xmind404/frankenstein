#include "run_command.h"

#include "build_command.h"
#include "../cli/console.h"
#include "../configuration/project_root.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace {

namespace fs = std::filesystem;

std::optional<fs::path> findExecutable(const fs::path& buildDirectory, const std::string& projectName) {
  if (!fs::exists(buildDirectory)) {
    return std::nullopt;
  }
#ifdef _WIN32
  const std::string executableName = projectName + ".exe";
#else
  const std::string executableName = projectName;
#endif
  std::error_code error;
  for (fs::recursive_directory_iterator iterator(buildDirectory, fs::directory_options::skip_permission_denied, error), end;
       iterator != end; iterator.increment(error)) {
    if (error) {
      error.clear();
      continue;
    }
    if (iterator->is_regular_file(error) && iterator->path().filename() == executableName) {
      return iterator->path();
    }
  }
  return std::nullopt;
}

bool isSourceInput(const fs::path& path) {
  const std::string filename = path.filename().string();
  const std::string extension = path.extension().string();
  return filename == "CMakeLists.txt" ||
         filename == "Frankenstein.lock" ||
         filename == "Frankenstein.toml" ||
         extension == ".cmake" ||
         extension == ".c" ||
         extension == ".cc" ||
         extension == ".cpp" ||
         extension == ".cxx" ||
         extension == ".c++" ||
         extension == ".h" ||
         extension == ".hh" ||
         extension == ".hpp" ||
         extension == ".hxx" ||
         extension == ".h++" ||
         extension == ".ipp" ||
         extension == ".inl" ||
         extension == ".tpp" ||
         extension == ".inc" ||
         extension == ".toml";
}

bool isBuildCurrent(const fs::path& executable) {
  std::error_code error;
  const auto executableTime = fs::last_write_time(executable, error);
  if (error) {
    return false;
  }

  const auto isNewer = [&](const fs::path& input) {
    std::error_code timeError;
    const auto inputTime = fs::last_write_time(input, timeError);
    return timeError || inputTime > executableTime;
  };

  const fs::path cacheFile = executable.parent_path() / "CMakeCache.txt";
  if (!fs::exists(cacheFile, error) || error || isNewer(cacheFile)) {
    return false;
  }

  fs::directory_iterator rootIterator(".", error);
  const fs::directory_iterator rootEnd;
  while (rootIterator != rootEnd) {
    if (error) {
      return false;
    }
    const fs::path input = rootIterator->path();
    const bool regular = rootIterator->is_regular_file(error);
    if (error) {
      return false;
    }
    if (regular && isSourceInput(input) && isNewer(input)) {
      return false;
    }
    rootIterator.increment(error);
  }
  if (error) {
    return false;
  }

  const fs::path sourceDirectories[] = {"src", "include", "external", "tests"};
  for (const fs::path& directory : sourceDirectories) {
    if (!fs::exists(directory, error)) {
      if (error) {
        return false;
      }
      continue;
    }
    fs::recursive_directory_iterator iterator(directory, fs::directory_options::skip_permission_denied, error);
    const fs::recursive_directory_iterator end;
    while (iterator != end) {
      if (error) {
        return false;
      }
      const fs::path input = iterator->path();
      const bool isDirectory = iterator->is_directory(error);
      if (error) {
        return false;
      }
      const bool regular = isDirectory ? false : iterator->is_regular_file(error);
      if (error) {
        return false;
      }
      if ((isDirectory || (regular && isSourceInput(input))) && isNewer(input)) {
        return false;
      }
      iterator.increment(error);
    }
    if (error) {
      return false;
    }
  }

  return true;
}

std::string quoteArgument(const std::string& value) {
#ifdef _WIN32
  std::string out = "\"";
  for (char c : value) {
    if (c == '"') out += '\\';
    out += c;
  }
  return out + "\"";
#else
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  return out + "'";
#endif
}

}

int RunCommand::execute(const CLI& cli) {
  const fs::path root = configuration::requireProjectRoot();
  if (root.empty()) {
    return 1;
  }

  std::error_code error;
  if (!fs::exists(root / "CMakeLists.txt", error) || error) {
    Console::error("CMakeLists.txt not found in the current directory.");
    return 1;
  }

  const std::string projectName = root.filename().string();
  const std::string configuration = cli.hasFlag("--release") ? "Release" : "Debug";
  const fs::path buildDirectory = fs::path("build") / configuration;

  std::optional<fs::path> executable = findExecutable(buildDirectory, projectName);

  if (!executable || !isBuildCurrent(*executable)) {
    BuildCommand build;
    const int result = build.execute(cli);
    if (result != 0) {
      return result;
    }
    executable = findExecutable(buildDirectory, projectName);
  } else {
    Console::print("Build is up to date; running the existing executable.");
  }

  if (!executable) {
    Console::error("Built executable was not found. Make sure the CMake project name matches the directory name.");
    return 1;
  }

  std::string command = "\"" + executable->string() + "\"";
  for (const std::string& argument : cli.getArguments()) {
    command += " " + quoteArgument(argument);
  }

  return std::system(command.c_str());
}