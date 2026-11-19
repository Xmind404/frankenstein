
#include "project_template.h"

#include <array>
#include <fstream>
#include <string>

namespace {

  struct File {
    std::string name;
    std::string content;
  };

  std::string createCMakeLists(
    configuration::Language language,
    const std::string& standard,
    const std::string& source_file
  ) {
    const bool is_c = language == configuration::Language::C;

    const std::string language_name = is_c ? "C" : "CXX";
    const std::string standard_variable = is_c ? "C_STANDARD" : "CXX_STANDARD";

    return
      "cmake_minimum_required(VERSION 3.25)\n"
      "\n"
      "get_filename_component(FRANKENSTEIN_PROJECT_NAME \"${CMAKE_CURRENT_SOURCE_DIR}\" NAME)\n"
      "\n"
      "project(${FRANKENSTEIN_PROJECT_NAME} VERSION 1.0.0 LANGUAGES " + language_name + ")\n"
      "\n"
      "set(CMAKE_" + standard_variable + " " + standard + ")\n"
      "set(CMAKE_" + standard_variable + "_REQUIRED ON)\n"
      "set(CMAKE_" + language_name + "_EXTENSIONS OFF)\n"
      "\n"
      "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
      "\n"
      "add_executable(${PROJECT_NAME} " + source_file + ")\n";
  }

  std::string createClangFormat(
    configuration::Language language,
    const std::string& standard
  ) {
    std::string content =
      "BasedOnStyle: LLVM\n"
      "IndentWidth: 2\n"
      "TabWidth: 2\n"
      "UseTab: Never\n"
      "ColumnLimit: 120\n";

    if (language == configuration::Language::C) {
      content += "Language: C\n";
    } else {
      content +=
        "Language: Cpp\n"
        "Standard: c++" + standard + "\n";
    }

    return content;
  }

  std::string createMainSource(configuration::Language language) {
    if (language == configuration::Language::C) {
      return
        "#include <stdio.h>\n"
        "\n"
        "int main(void) {\n"
        "  printf(\"Hello, frankenstein!\\n\");\n"
        "\n"
        "  return 0;\n"
        "}\n";
    }

    return
      "#include <iostream>\n"
      "\n"
      "int main() {\n"
      "  std::cout << \"Hello, frankenstein!\" << '\\n';\n"
      "\n"
      "  return 0;\n"
      "}\n";
  }

}

bool ProjectTemplate::create(
  const std::filesystem::path& project_path,
  const configuration::ProjectConfiguration& configuration
) {
  const bool is_c = configuration.getLanguage() == configuration::Language::C;

  const std::string standard = is_c
    ? configuration::toString(configuration.getCStandard())
    : configuration::toString(configuration.getCppStandard());

  const std::string source_file = is_c ? "src/main.c" : "src/main.cpp";

  const std::array<File, 8> files = {{
    {
      "CMakeLists.txt",
      createCMakeLists(
        configuration.getLanguage(),
        standard,
        source_file
      )
    },
    {
      ".clangd",
      "CompileFlags:\n"
      "  CompilationDatabase: build\n"
    },
    {
      ".clang-format",
      createClangFormat(configuration.getLanguage(), standard)
    },
    {
      ".gitignore",
      "build/\n"
      "cmake-build-*/\n"
      "CMakeFiles/\n"
      "CMakeCache.txt\n"
      "compile_commands.json\n"
      "*.o\n"
      "*.a\n"
      "*.so\n"
      "*.exe\n"
      ".idea/\n"
      ".vscode/\n"
      ".DS_Store\n"
    },
    {
      "README.md",
      "# " + project_path.filename().string() + "\n"
      "\n"
      "A project created with Frankenstein.\n"
    },
    {
      "Frankenstein.toml",
      std::string("[project]\nname = \"") + project_path.filename().string() +
      "\"\nversion = \"0.1.0\"\nstandard = \"" + standard + "\"\n\n[dependencies]\n"
    },
    {
      source_file,
      createMainSource(configuration.getLanguage())
    },
    {
      "tests/.gitkeep",
      ""
    }
  }};

  for (const File& file : files) {
    const std::filesystem::path file_path = project_path / file.name;

    std::error_code error;

    if (error) return false;
    if (std::filesystem::exists(file_path, error)) continue;
    if (error) return false;

    std::ofstream output(file_path, std::ios::binary | std::ios::out);

    if (!output.is_open()) {
      return false;
    }

    output << file.content;

    if (!output.good()) {
      return false;
    }

    output.close();

    if (output.fail()) {
      return false;
    }
  }

  return true;
}
