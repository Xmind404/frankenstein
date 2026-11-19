#include "run_command.h"
#include "build_command.h"
#include "../cli/console.h"
#include "../configuration/project_standard_sync.h"
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace {

  namespace fs = std::filesystem;

  std::optional<fs::path> findExecutable(
    const fs::path& build_directory,
    const std::string& project_name
  ) {
    if (!fs::exists(build_directory)) {
      return std::nullopt;
    }

#ifdef _WIN32
    const std::string executable_name = project_name + ".exe";
#else
    const std::string executable_name = project_name;
#endif

    std::error_code error;
    for (fs::recursive_directory_iterator iterator(
        build_directory,
        fs::directory_options::skip_permission_denied,
        error),
      end;
      iterator != end;
      iterator.increment(error)) {
      if (error) {
        error.clear();
        continue;
      }

      if (iterator->is_regular_file(error) &&
        iterator->path().filename() == executable_name) {
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

  bool isBuildCurrent(
    const fs::path& executable
  ) {
    std::error_code error;
    const auto executable_time = fs::last_write_time(executable, error);

    if (error) {
      return false;
    }

    const auto is_newer_than_executable = [&](const fs::path& input) {
      std::error_code time_error;
      const auto input_time = fs::last_write_time(input, time_error);

      return time_error || input_time > executable_time;
    };

    const fs::path cache_file = executable.parent_path() / "CMakeCache.txt";
    const bool has_cache = fs::exists(cache_file, error);
    if (error || !has_cache || is_newer_than_executable(cache_file)) {
      return false;
    }

    fs::directory_iterator root_iterator(".", error);
    const fs::directory_iterator root_end;

    while (root_iterator != root_end) {
      if (error) {
        return false;
      }

      const fs::path input = root_iterator->path();
      const bool is_regular_file = root_iterator->is_regular_file(error);

      if (error) {
        return false;
      }

      if (is_regular_file && isSourceInput(input) &&
        is_newer_than_executable(input)) {
        return false;
      }

      root_iterator.increment(error);
    }

    if (error) {
      return false;
    }

    const fs::path source_directories[] = {
      "src",
      "source",
      "app",
      "lib",
      "include",
      "external",
      "tests",
      "cmake"
    };

    for (const fs::path& directory : source_directories) {
      if (!fs::exists(directory, error)) {
        if (error) {
          return false;
        }

        continue;
      }

      fs::recursive_directory_iterator iterator(
        directory,
        fs::directory_options::skip_permission_denied,
        error
      );
      const fs::recursive_directory_iterator end;

      while (iterator != end) {
        if (error) {
          return false;
        }

        const fs::path input = iterator->path();
        const bool is_directory = iterator->is_directory(error);

        if (error) {
          return false;
        }

        const bool is_regular_file = is_directory
          ? false
          : iterator->is_regular_file(error);

        if (error) {
          return false;
        }

        if ((is_directory || (is_regular_file && isSourceInput(input))) &&
          is_newer_than_executable(input)) {
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

}

int RunCommand::execute(const CLI& cli) {
  BuildCommand build;
  namespace fs = std::filesystem;
  std::string standard_sync_message;
  if (!configuration::synchronizeProjectStandard(
    fs::current_path(),
    standard_sync_message
  )) {
    Console::error(standard_sync_message);
    return 1;
  }
  if (!standard_sync_message.empty()) {
    Console::print(standard_sync_message);
  }

  const std::string name = fs::current_path().filename().string();
  const std::string config = cli.hasFlag("--release") ? "Release" : "Debug";
  const fs::path build_directory = fs::path("build") / config;
  std::optional<fs::path> executable = findExecutable(build_directory, name);

  if (!executable || !isBuildCurrent(*executable)) {
    const int result = build.execute(cli);

    if (result != 0) {
      return result;
    }

    executable = findExecutable(build_directory, name);
  } else {
    Console::print("Build is up to date; running the existing executable.");
  }

  if (!executable) {
    Console::error("Built executable was not found. Set the CMake target name to the project directory name.");
    return 1;
  }

  std::string command = "\"" + executable->string() + "\"";

  for (const auto& argument : cli.getArguments()) {
#ifdef _WIN32
    command += " \"" + argument + "\"";
#else
    command += " '";
    for (char c : argument) {
      if (c == '\'') {
        command += "'\\''";
      } else {
        command += c;
      }
    }
    command += "'";
#endif
  }

  return std::system(command.c_str());
}
