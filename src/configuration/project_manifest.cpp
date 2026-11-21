#include "project_manifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace configuration {

namespace {

std::string trim(const std::string& value) {
  std::size_t begin = 0;
  std::size_t end = value.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
  return value.substr(begin, end - begin);
}

std::string unquote(const std::string& value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::string quote(const std::string& value) {
  return "\"" + value + "\"";
}

}

bool manifestExists(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(path, error) && !error;
}

Manifest readManifest(const std::filesystem::path& path) {
  Manifest manifest;
  std::ifstream input(path);
  if (!input) {
    return manifest;
  }

  std::string section;
  std::string pending_files;
  bool in_files_array = false;
  std::string line;

  while (std::getline(input, line)) {
    const std::string stripped = trim(line);

    if (stripped.empty() || stripped.front() == '#') {
      continue;
    }

    if (in_files_array) {
      const std::size_t close = stripped.find(']');
      const std::string part = close == std::string::npos ? stripped : stripped.substr(0, close);
      pending_files += part;
      if (close != std::string::npos) {
        in_files_array = false;
        std::istringstream list(pending_files);
        std::string item;
        while (std::getline(list, item, ',')) {
          const std::string cleaned = unquote(trim(item));
          if (!cleaned.empty()) manifest.sources.push_back(cleaned);
        }
        pending_files.clear();
      }
      continue;
    }

    if (stripped.front() == '[') {
      const std::size_t close = stripped.find(']');
      if (close != std::string::npos) {
        section = stripped.substr(1, close - 1);
      }
      continue;
    }

    const std::size_t equal = stripped.find('=');
    if (equal == std::string::npos) {
      continue;
    }

    const std::string key = trim(stripped.substr(0, equal));
    const std::string value = trim(stripped.substr(equal + 1));

    if (section == "project") {
      if (key == "name") manifest.name = unquote(value);
      else if (key == "version") manifest.version = unquote(value);
      else if (key == "standard") manifest.standard = unquote(value);
    } else if (section == "sources" && key == "files") {
      const std::size_t open = value.find('[');
      if (open == std::string::npos) {
        continue;
      }
      const std::size_t close = value.find(']', open);
      if (close == std::string::npos) {
        in_files_array = true;
        pending_files = value.substr(open + 1);
      } else {
        const std::string inside = value.substr(open + 1, close - open - 1);
        std::istringstream list(inside);
        std::string item;
        while (std::getline(list, item, ',')) {
          const std::string cleaned = unquote(trim(item));
          if (!cleaned.empty()) manifest.sources.push_back(cleaned);
        }
      }
    } else if (section.rfind("dependencies.", 0) == 0) {
      const std::string dep_name = section.substr(std::string("dependencies.").size());
      auto iterator = std::find_if(
        manifest.dependencies.begin(),
        manifest.dependencies.end(),
        [&](const Dependency& dependency) { return dependency.name == dep_name; }
      );
      if (iterator == manifest.dependencies.end()) {
        Dependency dependency;
        dependency.name = dep_name;
        manifest.dependencies.push_back(dependency);
        iterator = manifest.dependencies.end() - 1;
      }
      if (key == "repository") iterator->repository = unquote(value);
      else if (key == "branch") iterator->branch = unquote(value);
      else if (key == "target") iterator->target = unquote(value);
    }
  }

  return manifest;
}

bool writeManifest(const std::filesystem::path& path, const Manifest& manifest) {
  std::ostringstream output;
  output << "[project]\n";
  output << "name = " << quote(manifest.name) << "\n";
  output << "version = " << quote(manifest.version) << "\n";
  output << "standard = " << quote(manifest.standard) << "\n";
  output << "\n[sources]\n";
  output << "files = [\n";
  for (const std::string& source : manifest.sources) {
    output << "  " << quote(source) << ",\n";
  }
  output << "]\n";

  for (const Dependency& dependency : manifest.dependencies) {
    output << "\n[dependencies." << dependency.name << "]\n";
    output << "repository = " << quote(dependency.repository) << "\n";
    output << "branch = " << quote(dependency.branch) << "\n";
    if (!dependency.target.empty()) {
      output << "target = " << quote(dependency.target) << "\n";
    }
  }

  const std::string temporary = path.string() + ".tmp";
  std::ofstream out(temporary, std::ios::trunc);
  if (!out) {
    return false;
  }
  out << output.str();
  out.close();
  if (!out) {
    return false;
  }

  std::error_code error;
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temporary, path, error);
  return !error;
}

}