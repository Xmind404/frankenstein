#include "project_sources.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace configuration {

namespace {

  std::string trim(const std::string& value) {
    std::size_t a = 0, b = value.size();
    while (a < b && std::isspace(static_cast<unsigned char>(value[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(value[b - 1]))) --b;
    return value.substr(a, b - a);
  }

  std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') return value.substr(1, value.size() - 2);
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') return value.substr(1, value.size() - 2);
    return value;
  }

  std::string toPosix(const std::string& value) {
    std::string out = value;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
  }

  bool hasSourceExtension(const std::string& value) {
    static const char* extensions[] = {".c", ".cc", ".cpp", ".cxx", ".c++", ".C"};
    for (const char* ext : extensions) {
      const std::size_t length = std::char_traits<char>::length(ext);
      if (value.size() >= length && value.compare(value.size() - length, length, ext) == 0) return true;
    }
    return false;
  }

  std::vector<std::string> parseSourcesFromToml(const std::string& content) {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string line;
    bool in_sources = false;
    bool in_files = false;
    std::string pending;
    while (std::getline(stream, line)) {
      const auto stripped = trim(line);
      if (!stripped.empty() && stripped.front() == '[') {
        in_sources = stripped == "[sources]";
        in_files = false;
        pending.clear();
        continue;
      }
      if (!in_sources) continue;
      if (!in_files) {
        const auto eq = stripped.find('=');
        if (eq == std::string::npos) continue;
        if (trim(stripped.substr(0, eq)) != "files") continue;
        pending = stripped.substr(eq + 1);
        if (pending.find('[') != std::string::npos) in_files = pending.find(']') == std::string::npos;
        const auto open = pending.find('[');
        const auto close = pending.find(']');
        if (open != std::string::npos && close != std::string::npos) {
          pending = pending.substr(open + 1, close - open - 1);
          in_files = false;
        } else {
          if (open != std::string::npos) pending = pending.substr(open + 1);
          continue;
        }
      } else {
        if (stripped.find(']') != std::string::npos) {
          pending += stripped.substr(0, stripped.find(']'));
          in_files = false;
        } else {
          pending += stripped;
          continue;
        }
      }
      std::string item;
      std::istringstream list(pending);
      while (std::getline(list, item, ',')) {
        const auto cleaned = unquote(trim(item));
        if (!cleaned.empty()) result.push_back(cleaned);
      }
      pending.clear();
    }
    return result;
  }

  bool rewriteManifest(const std::filesystem::path& manifest, const std::vector<std::string>& sources) {
    std::ifstream input(manifest);
    if (!input) return false;
    std::vector<std::string> lines;
    std::string line;
    bool in_sources = false;
    bool inserted = false;
    bool had_sources = false;
    while (std::getline(input, line)) {
      const auto stripped = trim(line);
      if (!stripped.empty() && stripped.front() == '[') {
        if (in_sources && !inserted) {
          lines.push_back("files = [");
          for (const auto& source : sources) lines.push_back("  \"" + source + "\",");
          lines.push_back("]");
          inserted = true;
        }
        in_sources = stripped == "[sources]";
        if (in_sources) had_sources = true;
      }
      if (in_sources) continue;
      lines.push_back(line);
    }
    input.close();
    if (in_sources && !inserted) {
      lines.push_back("files = [");
      for (const auto& source : sources) lines.push_back("  \"" + source + "\",");
      lines.push_back("]");
      inserted = true;
    }
    if (!had_sources && !inserted) {
      lines.push_back("");
      lines.push_back("[sources]");
      lines.push_back("files = [");
      for (const auto& source : sources) lines.push_back("  \"" + source + "\",");
      lines.push_back("]");
    }
    std::ostringstream output;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      if (i != 0) output << '\n';
      output << lines[i];
    }
    const auto temporary = manifest.string() + ".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    if (!out) return false;
    out << output.str();
    out.close();
    if (!out) return false;
    std::error_code error;
    std::filesystem::remove(manifest, error);
    error.clear();
    std::filesystem::rename(temporary, manifest, error);
    return !error;
  }

  }

  bool isSourceFileArgument(const std::string& argument) {
    return hasSourceExtension(argument);
  }

  std::string normalizeSourcePath(const std::filesystem::path& project_root, const std::string& argument) {
    std::filesystem::path path = argument;
    if (path.is_absolute()) {
      std::error_code error;
      const auto relative = std::filesystem::relative(path, project_root, error);
      if (error) return {};
      path = relative;
    }
    const auto normalized = path.lexically_normal();
    if (normalized.empty()) return {};
    for (const auto& part : normalized) {
      if (part == "..") return {};
    }
    return toPosix(normalized.string());
  }

  std::vector<std::string> readSources(const std::filesystem::path& manifest) {
    std::ifstream input(manifest);
    if (!input) return {};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseSourcesFromToml(buffer.str());
  }

  bool writeSources(const std::filesystem::path& manifest, const std::vector<std::string>& sources) {
    return rewriteManifest(manifest, sources);
  }

  bool generateSourcesCmake(const std::filesystem::path& project_root, const std::vector<std::string>& sources) {
    const auto target = project_root / "frankenstein.sources.cmake";
    std::ostringstream output;
    output << "set(FRANKENSTEIN_SOURCES";
    for (const auto& source : sources) output << "\n  \"" << source << "\"";
    output << "\n)";
    const auto temporary = target.string() + ".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    if (!out) return false;
    out << output.str();
    out.close();
    if (!out) return false;
    std::error_code error;
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    return !error;
  }

}