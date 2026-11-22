#include "project_cmake_sync.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace configuration {

namespace {

namespace fs = std::filesystem;

std::string trim(const std::string& value) {
  std::size_t begin = 0;
  std::size_t end = value.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
  return value.substr(begin, end - begin);
}

bool writeFileAtomic(const fs::path& path, const std::string& content) {
  const fs::path temporary = path.string() + ".frankenstein.tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    if (!out) {
      std::error_code ignored;
      fs::remove(temporary, ignored);
      return false;
    }
  }

  std::error_code error;
  fs::rename(temporary, path, error);
  if (!error) return true;

  error.clear();
  if (!fs::exists(path, error) || error) {
    fs::remove(temporary, error);
    return false;
  }
  const fs::path backup = path.string() + ".frankenstein.bak";
  fs::remove(backup, error);
  error.clear();
  fs::rename(path, backup, error);
  if (error) {
    fs::remove(temporary, error);
    return false;
  }
  error.clear();
  fs::rename(temporary, path, error);
  if (error) {
    std::error_code restoreError;
    fs::rename(backup, path, restoreError);
    fs::remove(temporary, restoreError);
    return false;
  }
  fs::remove(backup, error);
  return true;
}

std::string renderSourcesBlock(const std::vector<std::string>& sources) {
  std::ostringstream block;
  block << "# BEGIN FRANKENSTEIN SOURCES\n";
  block << "set(FRANKENSTEIN_SOURCES";
  for (const std::string& source : sources) {
    block << "\n  \"" << source << "\"";
  }
  block << "\n)\n";
  block << "# END FRANKENSTEIN SOURCES";
  return block.str();
}

bool removeSourcesBlock(std::string& content) {
  const std::string begin = "# BEGIN FRANKENSTEIN SOURCES";
  const std::string end = "# END FRANKENSTEIN SOURCES";
  const std::size_t start = content.find(begin);
  if (start == std::string::npos) {
    return false;
  }
  const std::size_t finish = content.find(end, start);
  if (finish == std::string::npos) {
    return false;
  }
  std::size_t lineEnd = content.find('\n', finish);
  if (lineEnd == std::string::npos) {
    lineEnd = content.size();
  }
  content.erase(start, lineEnd - start + 1);
  return true;
}

void insertSourcesBlock(std::string& content, const std::string& block) {
  const std::size_t projectBegin = content.find("project(");
  if (projectBegin == std::string::npos) {
    content = block + "\n\n" + content;
    return;
  }
  const std::size_t projectEnd = content.find(')', projectBegin);
  if (projectEnd == std::string::npos) {
    content = block + "\n\n" + content;
    return;
  }
  const std::size_t lineEnd = content.find('\n', projectEnd);
  if (lineEnd == std::string::npos) {
    content += "\n\n" + block;
    return;
  }
  content.insert(lineEnd + 1, "\n" + block + "\n");
}

std::string renderDependencyBlock(const Dependency& dependency) {
  std::ostringstream block;
  block << "# BEGIN FRANKENSTEIN DEPENDENCY " << dependency.name << "\n";
  block << "add_subdirectory(external/" << dependency.name << " EXCLUDE_FROM_ALL)\n";
  block << "if(NOT TARGET " << dependency.target << ")\n";
  block << "  message(FATAL_ERROR \"Frankenstein: expected CMake target '" << dependency.target << "' from external/" << dependency.name << "'.\")\n";
  block << "endif()\n";
  block << "target_link_libraries(${PROJECT_NAME} PRIVATE " << dependency.target << ")\n";
  block << "# END FRANKENSTEIN DEPENDENCY " << dependency.name;
  return block.str();
}

void removeDependencyBlocks(std::string& content) {
  while (true) {
    const std::size_t begin = content.find("# BEGIN FRANKENSTEIN DEPENDENCY ");
    if (begin == std::string::npos) {
      return;
    }
    const std::size_t end = content.find("# END FRANKENSTEIN DEPENDENCY ", begin);
    if (end == std::string::npos) {
      return;
    }
    std::size_t lineEnd = content.find('\n', end);
    if (lineEnd == std::string::npos) {
      lineEnd = content.size();
    }
    content.erase(begin, lineEnd - begin + 1);
  }
}

void removeLinkingBlocks(std::string& content) {
  const std::string beginMarker = "# BEGIN FRANKENSTEIN LINKING";
  const std::string endMarker = "# END FRANKENSTEIN LINKING";
  while (true) {
    const std::size_t begin = content.find(beginMarker);
    if (begin == std::string::npos) return;
    const std::size_t end = content.find(endMarker, begin);
    if (end == std::string::npos) return;
    std::size_t lineEnd = content.find('\n', end);
    if (lineEnd == std::string::npos) lineEnd = content.size();
    content.erase(begin, lineEnd - begin + 1);
  }
}

std::string renderLinkLibrariesCall(const std::vector<std::string>& libraries) {
  std::ostringstream block;
  block << "target_link_libraries(${PROJECT_NAME} PRIVATE";
  for (const std::string& library : libraries) {
    block << "\n  " << library;
  }
  block << "\n)";
  return block.str();
}

std::string renderLinkingBlock(const Manifest& manifest) {
  std::ostringstream block;
  block << "# BEGIN FRANKENSTEIN LINKING\n";

  if (!manifest.linkLibraries.empty()) {
    block << renderLinkLibrariesCall(manifest.linkLibraries) << "\n";
  }
  if (!manifest.windowsLinkLibraries.empty()) {
    block << "if(WIN32)\n";
    block << "  " << renderLinkLibrariesCall(manifest.windowsLinkLibraries) << "\n";
    block << "endif()\n";
  }
  if (!manifest.linuxLinkLibraries.empty()) {
    block << "if(UNIX AND NOT APPLE)\n";
    block << "  " << renderLinkLibrariesCall(manifest.linuxLinkLibraries) << "\n";
    block << "endif()\n";
  }
  if (!manifest.macosLinkLibraries.empty()) {
    block << "if(APPLE)\n";
    block << "  " << renderLinkLibrariesCall(manifest.macosLinkLibraries) << "\n";
    block << "endif()\n";
  }

  block << "# END FRANKENSTEIN LINKING";
  return block.str();
}

enum class ProjectLanguage { C, Cpp };

bool readManifestStandard(const fs::path& manifest, std::string& standard) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string section;
  std::string line;
  while (std::getline(input, line)) {
    const std::string stripped = trim(line);
    if (stripped.empty() || stripped.front() == '#') continue;
    if (stripped.front() == '[') {
      const std::size_t close = stripped.find(']');
      section = close == std::string::npos ? std::string{} : trim(stripped.substr(1, close - 1));
      continue;
    }
    if (section != "project") continue;
    const std::size_t equal = stripped.find('=');
    if (equal == std::string::npos || trim(stripped.substr(0, equal)) != "standard") continue;
    std::string value = trim(stripped.substr(equal + 1));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\''))) {
      value = value.substr(1, value.size() - 2);
    }
    standard = trim(value);
    return !standard.empty();
  }
  return false;
}

bool detectProjectLanguage(const std::string& content, const std::string& standard,
                           ProjectLanguage& language, std::string& errorMessage) {
  std::string uncommented;
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);
    uncommented += line + '\n';
  }

  std::smatch match;
  const std::regex projectCommand(R"(project\s*\(([\s\S]*?)\))", std::regex::icase);
  if (!std::regex_search(uncommented, match, projectCommand)) {
    errorMessage = "Could not detect project() in CMakeLists.txt.";
    return false;
  }

  std::string args = match[1].str();
  std::transform(args.begin(), args.end(), args.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  const bool hasLanguagesKeyword = args.find("LANGUAGES") != std::string::npos;
  bool hasC = false;
  bool hasCpp = false;
  if (hasLanguagesKeyword) {
    const std::size_t pos = args.find("LANGUAGES") + 8;
    std::istringstream tokens(args.substr(pos));
    std::string token;
    while (tokens >> token) {
      if (token == "C") hasC = true;
      if (token == "CXX") hasCpp = true;
    }
  } else {
    hasC = true;
    hasCpp = true;
  }

  const bool cStandard = standard == "90" || standard == "99" || standard == "11" ||
                         standard == "17" || standard == "23";
  const bool cppStandard = standard == "98" || standard == "11" ||
                           standard == "14" || standard == "17" || standard == "20" ||
                           standard == "23" || standard == "26";

  if (hasC && !hasCpp) {
    if (!cStandard) {
      errorMessage = "Frankenstein.toml standard '" + standard +
        "' is not a valid C standard, but CMakeLists.txt enables only C.";
      return false;
    }
    language = ProjectLanguage::C;
    return true;
  }
  if (hasCpp && !hasC) {
    if (!cppStandard) {
      errorMessage = "Frankenstein.toml standard '" + standard +
        "' is not a valid C++ standard, but CMakeLists.txt enables only CXX.";
      return false;
    }
    language = ProjectLanguage::Cpp;
    return true;
  }
  if (!hasC && !hasCpp) {
    errorMessage = "CMakeLists.txt project() does not enable C or CXX.";
    return false;
  }
  if (cppStandard) {
    language = ProjectLanguage::Cpp;
    return true;
  }
  if (cStandard) {
    language = ProjectLanguage::C;
    return true;
  }
  errorMessage = "Frankenstein.toml contains an unsupported C/C++ standard: " + standard;
  return false;
}

bool replaceStandardInCmake(std::string& content, const std::string& standard, ProjectLanguage language) {
  const std::string variable = language == ProjectLanguage::C ? "CMAKE_C_STANDARD" : "CMAKE_CXX_STANDARD";
  const std::regex setting("(^|\\n)([ \\t]*set\\s*\\(\\s*" + variable + "\\s+)([^)\\s]+)", std::regex::icase);
  std::smatch match;
  if (std::regex_search(content, match, setting)) {
    const std::size_t valueStart = static_cast<std::size_t>(match.position(3));
    const std::size_t valueLength = static_cast<std::size_t>(match.length(3));
    if (content.compare(valueStart, valueLength, standard) == 0) return false;
    content.replace(valueStart, valueLength, standard);
    return true;
  }
  return false;
}

bool hasStandardSetting(const std::string& content, ProjectLanguage language) {
  const std::string variable = language == ProjectLanguage::C ? "CMAKE_C_STANDARD" : "CMAKE_CXX_STANDARD";
  const std::regex setting("(^|\\n)[ \\t]*set\\s*\\(\\s*" + variable + "\\b", std::regex::icase);
  return std::regex_search(content, setting);
}

void insertStandardBlock(std::string& content, const std::string& standard, ProjectLanguage language) {
  const std::string variable = language == ProjectLanguage::C ? "C" : "CXX";
  const std::string block =
    "set(CMAKE_" + variable + "_STANDARD " + standard + ")\n"
    "set(CMAKE_" + variable + "_STANDARD_REQUIRED ON)\n"
    "set(CMAKE_" + variable + "_EXTENSIONS OFF)\n";
  const std::size_t projectBegin = content.find("project(");
  if (projectBegin == std::string::npos) {
    content.insert(0, block + "\n");
    return;
  }
  const std::size_t projectEnd = content.find(')', projectBegin);
  if (projectEnd == std::string::npos) {
    content.insert(0, block + "\n");
    return;
  }
  const std::size_t lineEnd = content.find('\n', projectEnd);
  if (lineEnd == std::string::npos) content += "\n" + block;
  else content.insert(lineEnd + 1, "\n" + block);
}

bool replaceClangFormatStandard(std::string& content, const std::string& standard) {
  std::istringstream input(content);
  std::ostringstream output;
  std::string line;
  bool cppLanguage = false;
  bool standardFound = false;
  bool changed = false;
  while (std::getline(input, line)) {
    const std::string current = trim(line);
    if (current == "Language: Cpp") {
      cppLanguage = true;
    } else if (!current.empty() && current.front() == '[') {
      cppLanguage = false;
    }
    if (cppLanguage && current.compare(0, std::string("Standard: c++").size(), "Standard: c++") == 0) {
      const std::size_t valueBegin = line.find("c++") + 3;
      const std::size_t valueEnd = line.find_first_of(" \t#", valueBegin);
      const std::size_t end = valueEnd == std::string::npos ? line.size() : valueEnd;
      standardFound = true;
      const std::string current_value = line.substr(valueBegin, end - valueBegin);
      if (current_value != standard) {
        line.replace(valueBegin, end - valueBegin, standard);
        changed = true;
      }
    }
    output << line << '\n';
  }
  if (cppLanguage && !standardFound) {
    output << "Standard: c++" << standard << '\n';
    changed = true;
  }
  content = output.str();
  return changed;
}

}

bool synchronizeStandard(const fs::path& root, std::string& message) {
  const fs::path manifestPath = root / "Frankenstein.toml";
  const fs::path cmakePath = root / "CMakeLists.txt";
  std::error_code error;
  if (!fs::is_regular_file(manifestPath, error) || error) {
    message = "Frankenstein.toml is missing or is not a regular file.";
    return false;
  }
  error.clear();
  if (!fs::is_regular_file(cmakePath, error) || error) {
    message = "CMakeLists.txt is missing or is not a regular file.";
    return false;
  }

  std::string standard;
  if (!readManifestStandard(manifestPath, standard)) {
    message = "Could not read a non-empty [project].standard from Frankenstein.toml.";
    return false;
  }

  std::ifstream cmakeInput(cmakePath, std::ios::binary);
  if (!cmakeInput) {
    message = "Could not open CMakeLists.txt.";
    return false;
  }
  std::string cmakeContent{std::istreambuf_iterator<char>(cmakeInput), std::istreambuf_iterator<char>{}};
  cmakeInput.close();

  ProjectLanguage language{};
  if (!detectProjectLanguage(cmakeContent, standard, language, message)) return false;

  bool cmakeChanged = replaceStandardInCmake(cmakeContent, standard, language);
  if (!hasStandardSetting(cmakeContent, language)) {
    insertStandardBlock(cmakeContent, standard, language);
    cmakeChanged = true;
  }

  bool formatChanged = false;
  if (language == ProjectLanguage::Cpp) {
    const fs::path formatPath = root / ".clang-format";
    error.clear();
    if (fs::is_regular_file(formatPath, error) && !error) {
      std::ifstream formatInput(formatPath, std::ios::binary);
      if (!formatInput) {
        message = "Could not open .clang-format.";
        return false;
      }
      std::string formatContent{std::istreambuf_iterator<char>(formatInput), std::istreambuf_iterator<char>{}};
      formatChanged = replaceClangFormatStandard(formatContent, standard);
      if (formatChanged && !writeFileAtomic(formatPath, formatContent)) {
        message = "Could not safely update .clang-format.";
        return false;
      }
    }
  }

  if (cmakeChanged && !writeFileAtomic(cmakePath, cmakeContent)) {
    message = "Could not safely update CMakeLists.txt.";
    return false;
  }
  if (cmakeChanged || formatChanged) {
    message = "Synchronized " + std::string(language == ProjectLanguage::C ? "C" : "C++") +
      " standard to " + standard + ".";
  } else {
    message.clear();
  }
  return true;
}

bool synchronizeSources(const fs::path& root, const std::vector<std::string>& sources) {
  const fs::path cmakePath = root / "CMakeLists.txt";
  std::ifstream input(cmakePath);
  if (!input) {
    return false;
  }
  std::string content{
    std::istreambuf_iterator<char>(input),
    std::istreambuf_iterator<char>{}
  };
  input.close();
  removeSourcesBlock(content);
  insertSourcesBlock(content, renderSourcesBlock(sources));
  return writeFileAtomic(cmakePath, content);
}

bool validCmakeTarget(const std::string& target) {
  if (target.empty()) return true;
  std::size_t start = 0;
  while (start < target.size()) {
    const std::size_t separator = target.find("::", start);
    const std::string component = target.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
    if (component.empty() || !std::all_of(component.begin(), component.end(), [](unsigned char c) {
          return std::isalnum(c) || c == '_' || c == '.' || c == '+' || c == '-';
        })) return false;
    if (separator == std::string::npos) return true;
    start = separator + 2;
    if (start == target.size()) return false;
  }
  return false;
}

bool synchronizeDependencies(const fs::path& root, const std::vector<Dependency>& dependencies) {
  const fs::path cmakePath = root / "CMakeLists.txt";
  std::ifstream input(cmakePath);
  if (!input) return false;

  for (const Dependency& dependency : dependencies) {
    if (!validCmakeTarget(dependency.target)) return false;
    if (dependency.target.empty()) continue;
    const std::string name = dependency.name.empty()
      ? dependency.repository.substr(dependency.repository.find('/') == std::string::npos
          ? 0 : dependency.repository.find('/') + 1)
      : dependency.name;
    if (name.empty() || name == "." || name == ".." ||
        name.find_first_of("/\\") != std::string::npos) return false;
    std::error_code error;
    if (!fs::is_regular_file(root / "external" / name / "CMakeLists.txt", error) || error) return false;
  }

  std::string content{
    std::istreambuf_iterator<char>(input),
    std::istreambuf_iterator<char>{}
  };
  input.close();
  removeDependencyBlocks(content);

  std::ostringstream blocks;
  for (const Dependency& dependency : dependencies) {
    if (dependency.target.empty()) continue;
    blocks << "\n" << renderDependencyBlock(dependency);
  }
  content += blocks.str();
  if (!content.empty() && content.back() == '\n') content.pop_back();
  return writeFileAtomic(cmakePath, content);
}

bool synchronizeLinking(const fs::path& root,
                        const Manifest& manifest,
                        std::string& message) {
  const fs::path cmakePath = root / "CMakeLists.txt";
  std::ifstream input(cmakePath);
  if (!input) {
    message = "Could not open CMakeLists.txt.";
    return false;
  }
  std::string content{
    std::istreambuf_iterator<char>(input),
    std::istreambuf_iterator<char>{}
  };
  input.close();

  removeLinkingBlocks(content);

  const bool hasAnyLinking =
      !manifest.linkLibraries.empty() ||
      !manifest.windowsLinkLibraries.empty() ||
      !manifest.linuxLinkLibraries.empty() ||
      !manifest.macosLinkLibraries.empty();

  if (hasAnyLinking) {
    if (!content.empty() && content.back() != '\n') content += '\n';
    content += "\n" + renderLinkingBlock(manifest) + "\n";
    if (!content.empty() && content.back() == '\n') content.pop_back();
  } else if (!content.empty() && content.back() == '\n') {
    content.pop_back();
  }

  if (!writeFileAtomic(cmakePath, content)) {
    message = "Could not safely update CMakeLists.txt (linking block).";
    return false;
  }
  message.clear();
  return true;
}

}