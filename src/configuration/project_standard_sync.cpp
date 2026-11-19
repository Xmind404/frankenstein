#include "project_standard_sync.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace {

  std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
      return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
  }

  bool readManifestStandard(
    const std::filesystem::path& manifest_path,
    std::string& standard,
    bool& found,
    std::string& error_message
  ) {
    std::ifstream manifest(manifest_path);

    if (!manifest.is_open()) {
      error_message = "Could not open Frankenstein.toml.";
      return false;
    }

    bool in_project_section = false;
    std::string line;

    while (std::getline(manifest, line)) {
      const std::string current = trim(line);

      if (!current.empty() && current.front() == '[') {
        in_project_section = current == "[project]";
        continue;
      }

      if (!in_project_section) {
        continue;
      }

      const std::size_t separator = current.find('=');

      if (separator == std::string::npos ||
        trim(current.substr(0, separator)) != "standard") {
        continue;
      }

      std::string value = trim(current.substr(separator + 1));
      const std::size_t comment = value.find('#');

      if (comment != std::string::npos) {
        value = trim(value.substr(0, comment));
      }

      if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
      }

      if (value.empty() ||
        !std::all_of(value.begin(), value.end(), [](unsigned char character) {
          return std::isdigit(character) != 0;
        })) {
        error_message = "The project standard in Frankenstein.toml must be a number.";
        return false;
      }

      standard = value;
      found = true;
      return true;
    }

    if (manifest.bad()) {
      error_message = "Could not read Frankenstein.toml.";
      return false;
    }

    return true;
  }

  bool replaceStandard(
    std::string& contents,
    const std::string& variable,
    const std::string& standard,
    bool& found,
    bool& changed
  ) {
    const std::string prefix = "set(" + variable + " ";
    std::istringstream input(contents);
    std::ostringstream output;
    std::string line;

    while (std::getline(input, line)) {
      const std::size_t first = line.find_first_not_of(" \t");

      if (first != std::string::npos &&
        line.compare(first, prefix.size(), prefix) == 0) {
        const std::size_t value_begin = first + prefix.size();
        const std::size_t value_end = line.find(')', value_begin);

        if (value_end != std::string::npos) {
          found = true;

          if (line.substr(value_begin, value_end - value_begin) != standard) {
            line.replace(value_begin, value_end - value_begin, standard);
            changed = true;
          }
        }
      }

      output << line << '\n';
    }

    contents = output.str();
    return !input.bad();
  }

  bool isCProject(
    const std::filesystem::path& project_root,
    const std::string& cmake_contents
  ) {
    const std::size_t project_begin = cmake_contents.find("project(");

    if (project_begin != std::string::npos) {
      const std::size_t project_end = cmake_contents.find(')', project_begin);

      if (project_end != std::string::npos) {
        const std::string project_command = cmake_contents.substr(
          project_begin,
          project_end - project_begin + 1
        );

        if (project_command.find("LANGUAGES CXX") != std::string::npos ||
          project_command.find("LANGUAGES C CXX") != std::string::npos ||
          project_command.find(" LANGUAGES CXX") != std::string::npos) {
          return false;
        }

        if (project_command.find("LANGUAGES C)") != std::string::npos ||
          project_command.find("LANGUAGES C ") != std::string::npos) {
          return true;
        }
      }
    }

    const bool has_c_source = std::filesystem::exists(project_root / "src/main.c");
    const bool has_cpp_source = std::filesystem::exists(project_root / "src/main.cpp");

    return has_c_source && !has_cpp_source;
  }

  void insertStandard(
    std::string& contents,
    const std::string& standard,
    bool is_c
  ) {
    const std::string variable = is_c ? "C" : "CXX";
    const std::string block =
      "set(CMAKE_" + variable + "_STANDARD " + standard + ")\n"
      "set(CMAKE_" + variable + "_STANDARD_REQUIRED ON)\n"
      "set(CMAKE_" + variable + "_EXTENSIONS OFF)\n";

    const std::size_t project_begin = contents.find("project(");

    if (project_begin == std::string::npos) {
      contents.insert(0, block + "\n");
      return;
    }

    const std::size_t project_end = contents.find(')', project_begin);

    if (project_end == std::string::npos) {
      contents.insert(0, block + "\n");
      return;
    }

    const std::size_t line_end = contents.find('\n', project_end);
    const std::size_t insert_at = line_end == std::string::npos
      ? contents.size()
      : line_end + 1;

    contents.insert(insert_at, "\n" + block);
  }

  bool replaceClangFormatStandard(
    std::string& contents,
    const std::string& standard,
    bool& changed
  ) {
    std::istringstream input(contents);
    std::ostringstream output;
    std::string line;
    bool cpp_language = false;
    bool standard_found = false;

    while (std::getline(input, line)) {
      const std::string current = trim(line);

      if (current == "Language: Cpp") {
        cpp_language = true;
      } else if (!current.empty() && current.front() == '[') {
        cpp_language = false;
      }

      if (cpp_language && current.starts_with("Standard: c++")) {
        const std::size_t value_begin = line.find("c++") + 3;
        const std::size_t value_end = line.find_first_of(" \t#", value_begin);
        const std::size_t end = value_end == std::string::npos
          ? line.size()
          : value_end;

        standard_found = true;

        if (line.substr(value_begin, end - value_begin) != standard) {
          line.replace(value_begin, end - value_begin, standard);
          changed = true;
        }
      }

      output << line << '\n';
    }

    if (cpp_language && !standard_found) {
      output << "Standard: c++" << standard << '\n';
      changed = true;
    }

    contents = output.str();
    return !input.bad();
  }

  bool writeFile(
    const std::filesystem::path& path,
    const std::string& contents,
    std::string& error_message
  ) {
    std::filesystem::path temporary_path = path;
    temporary_path += ".frankenstein.tmp";

    {
      std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);

      if (!output.is_open()) {
        error_message = "Could not write " + path.filename().string() + ".";
        return false;
      }

      output << contents;
      output.close();

      if (output.fail()) {
        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);
        error_message = "Could not write " + path.filename().string() + ".";
        return false;
      }
    }

    std::error_code error;
    std::filesystem::copy_file(
      temporary_path,
      path,
      std::filesystem::copy_options::overwrite_existing,
      error
    );

    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);

    if (error) {
      error_message = "Could not update " + path.filename().string() +
        ": " + error.message();
      return false;
    }

    return true;
  }

}

namespace configuration {

  bool synchronizeProjectStandard(
    const std::filesystem::path& project_root,
    std::string& message
  ) {
    const std::filesystem::path manifest_path = project_root / "Frankenstein.toml";
    std::error_code error;

    if (!std::filesystem::exists(manifest_path, error)) {
      if (error) {
        message = "Could not inspect Frankenstein.toml: " + error.message();
        return false;
      }

      return true;
    }

    std::string standard;
    std::string error_message;
    bool has_standard = false;

    if (!readManifestStandard(
      manifest_path,
      standard,
      has_standard,
      error_message
    )) {
      message = error_message;
      return false;
    }

    if (!has_standard) {
      return true;
    }

    const std::filesystem::path cmake_path = project_root / "CMakeLists.txt";
    std::ifstream cmake_input(cmake_path, std::ios::binary);

    if (!cmake_input.is_open()) {
      message = "Could not open CMakeLists.txt.";
      return false;
    }

    std::string cmake_contents{
      std::istreambuf_iterator<char>(cmake_input),
      std::istreambuf_iterator<char>{}
    };

    bool cmake_changed = false;
    bool standard_found = false;

    if (!replaceStandard(
      cmake_contents,
      "CMAKE_CXX_STANDARD",
      standard,
      standard_found,
      cmake_changed
    )) {
      message = "Could not read CMakeLists.txt.";
      return false;
    }

    if (!standard_found &&
      !replaceStandard(
        cmake_contents,
        "CMAKE_C_STANDARD",
        standard,
        standard_found,
        cmake_changed
      )) {
      message = "Could not read CMakeLists.txt.";
      return false;
    }

    if (!standard_found) {
      insertStandard(
        cmake_contents,
        standard,
        isCProject(project_root, cmake_contents)
      );
      standard_found = true;
      cmake_changed = true;
    }

    bool format_changed = false;
    const std::filesystem::path format_path = project_root / ".clang-format";

    if (std::filesystem::exists(format_path, error) && !error) {
      std::ifstream format_input(format_path, std::ios::binary);

      if (!format_input.is_open()) {
        message = "Could not open .clang-format.";
        return false;
      }

      std::string format_contents{
        std::istreambuf_iterator<char>(format_input),
        std::istreambuf_iterator<char>{}
      };

      if (!replaceClangFormatStandard(
        format_contents,
        standard,
        format_changed
      )) {
        message = "Could not read .clang-format.";
        return false;
      }

      if (format_changed &&
        !writeFile(format_path, format_contents, error_message)) {
        message = error_message;
        return false;
      }
    } else if (error) {
      message = "Could not inspect .clang-format: " + error.message();
      return false;
    }

    if (cmake_changed && !writeFile(cmake_path, cmake_contents, error_message)) {
      message = error_message;
      return false;
    }

    if (cmake_changed || format_changed) {
      message = "Synchronized project standard to " + standard +
        " from Frankenstein.toml.";
    }

    return true;
  }

}
