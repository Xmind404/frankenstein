#include "project_manifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

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

bool consumeArrayItems(std::istringstream& parts, std::string initial,
                       std::vector<std::string>& out) {
  std::string buffer = std::move(initial);
  while (true) {
    std::size_t comma = 0;
    while ((comma = buffer.find(',')) != std::string::npos) {
      const std::string item = trim(buffer.substr(0, comma));
      if (!item.empty()) out.push_back(unquote(item));
      buffer.erase(0, comma + 1);
    }
    const std::size_t close = buffer.find(']');
    if (close != std::string::npos) {
      const std::string item = trim(buffer.substr(0, close));
      if (!item.empty()) out.push_back(unquote(item));
      return true;
    }
    std::string line;
    if (!std::getline(parts, line)) {
      const std::string item = trim(buffer);
      if (!item.empty()) out.push_back(unquote(item));
      return false;
    }
    buffer += " " + line;
  }
}

bool parseInlineArray(const std::string& value, std::vector<std::string>& out,
                      bool& needsMore) {
  const std::size_t open = value.find('[');
  needsMore = false;
  if (open == std::string::npos) return false;
  const std::string inside = value.substr(open + 1);
  const std::size_t close = inside.find(']');
  if (close != std::string::npos) {
    std::istringstream list(inside.substr(0, close));
    std::string item;
    while (std::getline(list, item, ',')) {
      const std::string cleaned = unquote(trim(item));
      if (!cleaned.empty()) out.push_back(cleaned);
    }
    return true;
  }
  needsMore = true;
  std::istringstream list(inside);
  std::string item;
  while (std::getline(list, item, ',')) {
    const std::string cleaned = unquote(trim(item));
    if (!cleaned.empty()) out.push_back(cleaned);
  }
  return true;
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

  bool in_link_array = false;
  std::vector<std::string>* link_target = nullptr;

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

    if (in_link_array && link_target) {
      std::istringstream parts(stripped);
      if (consumeArrayItems(parts, stripped, *link_target)) {
        in_link_array = false;
        link_target = nullptr;
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
    } else if (section == "linking" ||
               section == "linking.windows" ||
               section == "linking.linux" ||
               section == "linking.macos") {
      if (key != "libraries") {
        continue;
      }
      std::vector<std::string>* target = nullptr;
      if (section == "linking") target = &manifest.linkLibraries;
      else if (section == "linking.windows") target = &manifest.windowsLinkLibraries;
      else if (section == "linking.linux") target = &manifest.linuxLinkLibraries;
      else target = &manifest.macosLinkLibraries;

      bool needsMore = false;
      if (parseInlineArray(value, *target, needsMore) && needsMore) {
        in_link_array = true;
        link_target = target;
      }
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

  const bool hasAnyLinking =
      !manifest.linkLibraries.empty() ||
      !manifest.windowsLinkLibraries.empty() ||
      !manifest.linuxLinkLibraries.empty() ||
      !manifest.macosLinkLibraries.empty();
  if (hasAnyLinking) {
    output << "\n[linking]\n";
    output << "libraries = [\n";
    for (const std::string& library : manifest.linkLibraries) {
      output << "  " << quote(library) << ",\n";
    }
    output << "]\n";

    if (!manifest.windowsLinkLibraries.empty()) {
      output << "\n[linking.windows]\n";
      output << "libraries = [\n";
      for (const std::string& library : manifest.windowsLinkLibraries) {
        output << "  " << quote(library) << ",\n";
      }
      output << "]\n";
    }
    if (!manifest.linuxLinkLibraries.empty()) {
      output << "\n[linking.linux]\n";
      output << "libraries = [\n";
      for (const std::string& library : manifest.linuxLinkLibraries) {
        output << "  " << quote(library) << ",\n";
      }
      output << "]\n";
    }
    if (!manifest.macosLinkLibraries.empty()) {
      output << "\n[linking.macos]\n";
      output << "libraries = [\n";
      for (const std::string& library : manifest.macosLinkLibraries) {
        output << "  " << quote(library) << ",\n";
      }
      output << "]\n";
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

std::string currentPlatformName() {
#ifdef _WIN32
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#else
  return "other";
#endif
}

std::vector<std::string> resolvedLinkLibraries(const Manifest& manifest,
                                               const std::string& platform) {
  std::vector<std::string> merged;
  merged.reserve(manifest.linkLibraries.size() +
                 manifest.windowsLinkLibraries.size() +
                 manifest.linuxLinkLibraries.size() +
                 manifest.macosLinkLibraries.size());

  const std::vector<std::string>* platformList = nullptr;
  if (platform == "windows") platformList = &manifest.windowsLinkLibraries;
  else if (platform == "linux") platformList = &manifest.linuxLinkLibraries;
  else if (platform == "macos") platformList = &manifest.macosLinkLibraries;

  auto pushUnique = [&merged](const std::string& library) {
    if (library.empty()) return;
    if (std::find(merged.begin(), merged.end(), library) == merged.end()) {
      merged.push_back(library);
    }
  };

  for (const std::string& library : manifest.linkLibraries) pushUnique(library);
  if (platformList) {
    for (const std::string& library : *platformList) pushUnique(library);
  }
  return merged;
}

std::vector<std::string> resolvedLinkLibraries(const Manifest& manifest) {
  return resolvedLinkLibraries(manifest, currentPlatformName());
}

}