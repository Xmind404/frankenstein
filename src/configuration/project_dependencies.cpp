#include "project_dependencies.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef FRANKENSTEIN_HAS_CPR
#include <cpr/cpr.h>
#endif

namespace configuration {

namespace {

namespace fs = std::filesystem;

std::string shellQuote(const std::string& value) {
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

bool isHex(const std::string& value, std::size_t length) {
  if (value.size() != length) return false;
  for (unsigned char c : value) {
    if (!std::isxdigit(c)) return false;
  }
  return true;
}

bool validBranchName(const std::string& branch) {
  if (branch.empty() || branch.front() == '/' || branch.back() == '/' ||
      branch.find("//") != std::string::npos || branch.find("..") != std::string::npos ||
      branch.find("@{") != std::string::npos || branch.back() == '.') return false;
  for (unsigned char c : branch) {
    if (c <= 0x20 || c == 0x7f || c == '~' || c == '^' || c == ':' || c == '?' ||
        c == '*' || c == '[' || c == '\\') return false;
  }
  std::istringstream parts(branch);
  std::string part;
  while (std::getline(parts, part, '/')) {
    if (part.empty() || part.front() == '.' ||
        (part.size() >= 5 && part.compare(part.size() - 5, 5, ".lock") == 0)) return false;
  }
  return true;
}

std::string encodeRef(const std::string& value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') result += static_cast<char>(c);
    else { result += '%'; result += hex[c >> 4]; result += hex[c & 0x0F]; }
  }
  return result;
}

bool hasSourceExtension(const std::string& value) {
  static const char* extensions[] = {".c", ".cc", ".cpp", ".cxx", ".c++", ".C"};
  for (const char* extension : extensions) {
    const std::size_t length = std::char_traits<char>::length(extension);
    if (value.size() >= length && value.compare(value.size() - length, length, extension) == 0) {
      return true;
    }
  }
  return false;
}

std::string toPosix(const std::string& value) {
  std::string out = value;
  std::replace(out.begin(), out.end(), '\\', '/');
  return out;
}

std::string sha256File(const fs::path& path) {
#ifdef _WIN32
  const std::string command = "cmake -E sha256sum " + shellQuote(path.string());
  FILE* pipe = _popen(command.c_str(), "r");
#else
  const std::string command = "cmake -E sha256sum " + shellQuote(path.string());
  FILE* pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) {
    return {};
  }
  char buffer[256]{};
  std::string output;
  while (std::fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  std::istringstream stream(output);
  std::string digest;
  stream >> digest;
  return digest;
}

bool resolveBranchCommit(
  const std::string& repository,
  const std::string& branch,
  std::string& commit,
  std::string& error
) {
#ifdef FRANKENSTEIN_HAS_CPR
  const std::size_t slash = repository.find('/');
  if (slash == std::string::npos) {
    error = "Invalid repository identifier: " + repository;
    return false;
  }
  const std::string owner = repository.substr(0, slash);
  const std::string name = repository.substr(slash + 1);
  const std::string url = "https://api.github.com/repos/" + owner + "/" + name + "/commits/" + encodeRef(branch);
  const cpr::Response response = cpr::Get(
    cpr::Url{url},
    cpr::Header{{"Accept", "application/vnd.github+json"}, {"User-Agent", "frankenstein"}},
    cpr::Redirect{true},
    cpr::VerifySsl{true},
    cpr::Timeout{30000}
  );
  if (response.error || response.status_code != 200) {
    error = "Could not resolve GitHub branch (HTTP " + std::to_string(response.status_code) + ").";
    return false;
  }
  const std::size_t key = response.text.find("\"sha\"");
  const std::size_t colon = key == std::string::npos ? key : response.text.find(':', key);
  const std::size_t first = colon == std::string::npos ? colon : response.text.find('"', colon);
  const std::size_t last = first == std::string::npos ? first : response.text.find('"', first + 1);
  if (last == std::string::npos) {
    error = "GitHub returned no commit SHA.";
    return false;
  }
  commit = response.text.substr(first + 1, last - first - 1);
  if (!isHex(commit, 40) && !isHex(commit, 64)) {
    error = "GitHub returned an invalid commit SHA.";
    return false;
  }
  return true;
#else
  (void)repository;
  (void)branch;
  (void)commit;
  error = "This Frankenstein build has no CPR support.";
  return false;
#endif
}

std::string buildTargetDetectionScript(const std::string& sourceDirectory) {
  std::string escaped;
  for (char c : sourceDirectory) {
    if (c == '"' || c == '\\') escaped += '\\';
    escaped += c;
  }
  std::ostringstream script;
  script << "cmake_minimum_required(VERSION 3.16)\n"
         << "project(frankenstein_target_probe LANGUAGES C CXX)\n"
         << "set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)\n"
         << "add_subdirectory(\"" << escaped << "\" \"${CMAKE_BINARY_DIR}/dep\")\n"
         << "file(WRITE \"${CMAKE_BINARY_DIR}/frankenstein_targets.txt\" \"\")\n"
         << "function(frankenstein_collect_targets dir out_var)\n"
         << "  set(_all)\n"
         << "  get_property(_bs DIRECTORY \"${dir}\" PROPERTY BUILDSYSTEM_TARGETS)\n"
         << "  get_property(_al DIRECTORY \"${dir}\" PROPERTY ALIASED_TARGETS)\n"
         << "  foreach(_t IN LISTS _bs _al)\n"
         << "    list(APPEND _all \"${_t}\")\n"
         << "  endforeach()\n"
         << "  get_property(_subs DIRECTORY \"${dir}\" PROPERTY SUBDIRECTORIES)\n"
         << "  foreach(_sub IN LISTS _subs)\n"
         << "    frankenstein_collect_targets(\"${_sub}\" _sub_targets)\n"
         << "    list(APPEND _all ${_sub_targets})\n"
         << "  endforeach()\n"
         << "  set(${out_var} ${_all} PARENT_SCOPE)\n"
         << "endfunction()\n"
         << "frankenstein_collect_targets(\"${CMAKE_SOURCE_DIR}\" _all_targets)\n"
         << "if(_all_targets)\n"
         << "  list(REMOVE_DUPLICATES _all_targets)\n"
         << "endif()\n"
         << "foreach(_t IN LISTS _all_targets)\n"
         << "  get_target_property(_type ${_t} TYPE)\n"
         << "  if(_type STREQUAL \"STATIC_LIBRARY\" OR _type STREQUAL \"SHARED_LIBRARY\" OR _type STREQUAL \"INTERFACE_LIBRARY\" OR _type STREQUAL \"OBJECT_LIBRARY\" OR _type STREQUAL \"UNKNOWN_LIBRARY\")\n"
         << "    file(APPEND \"${CMAKE_BINARY_DIR}/frankenstein_targets.txt\" \"${_t}|${_type}\\n\")\n"
         << "  endif()\n"
         << "endforeach()\n";
  return script.str();
}

}

bool isSourceFileArgument(const std::string& argument) {
  return hasSourceExtension(argument);
}

std::string normalizeSourcePath(const fs::path& root, const std::string& argument) {
  fs::path path = argument;
  if (path.is_absolute()) {
    std::error_code error;
    const fs::path relative = fs::relative(path, root, error);
    if (error) {
      return {};
    }
    path = relative;
  }
  const fs::path normalized = path.lexically_normal();
  if (normalized.empty()) {
    return {};
  }
  for (const fs::path& part : normalized) {
    if (part == "..") {
      return {};
    }
  }
  return toPosix(normalized.string());
}

std::string dependencyNameFromRepository(const std::string& repository) {
  const std::size_t slash = repository.find('/');
  if (slash == std::string::npos) {
    return repository;
  }
  return repository.substr(slash + 1);
}

bool dependencyExistsOnDisk(const fs::path& root, const std::string& repository) {
  const std::string name = dependencyNameFromRepository(repository);
  std::error_code error;
  return fs::is_directory(root / "external" / name, error) && !error;
}

bool removeDependencyFiles(const fs::path& root, const std::string& repository) {
  const std::string name = dependencyNameFromRepository(repository);
  std::error_code error;
  fs::remove_all(root / "external" / name, error);
  return !error;
}

std::vector<TargetInfo> detectTargets(const fs::path& root, const std::string& dependencyName) {
  std::vector<TargetInfo> result;
  const fs::path scratch = root / "external" / (".frankenstein-targets-" + dependencyName);
  std::error_code error;

  fs::remove_all(scratch, error);
  fs::create_directories(scratch, error);
  if (error) {
    return result;
  }

  const fs::path probe = scratch / "probe";
  fs::create_directories(probe, error);
  if (error) {
    fs::remove_all(scratch, error);
    return result;
  }

  const fs::path sourceDirectory = root / "external" / dependencyName;
  {
    std::ofstream script(probe / "CMakeLists.txt", std::ios::trunc);
    if (!script) {
      fs::remove_all(scratch, error);
      return result;
    }
    script << buildTargetDetectionScript(sourceDirectory.string());
  }

  const fs::path buildDirectory = scratch / "build";
  fs::create_directories(buildDirectory, error);
  if (error) {
    fs::remove_all(scratch, error);
    return result;
  }

  const fs::path log = scratch / "configure.log";
  const std::string command =
    "cmake -S " + shellQuote(probe.string()) +
    " -B " + shellQuote(buildDirectory.string()) +
    " -DCMAKE_BUILD_TYPE=Release > " + shellQuote(log.string()) + " 2>&1";

  const int status = std::system(command.c_str());
  if (status != 0) {
    fs::remove_all(scratch, error);
    return result;
  }

  std::ifstream list(buildDirectory / "frankenstein_targets.txt");
  std::string line;
  while (std::getline(list, line)) {
    const std::size_t separator = line.find('|');
    if (separator == std::string::npos) {
      continue;
    }
    TargetInfo info;
    info.name = line.substr(0, separator);
    info.type = line.substr(separator + 1);
    if (!info.name.empty()) {
      result.push_back(info);
    }
  }

  fs::remove_all(scratch, error);
  return result;
}

DependencyInstallResult downloadDependency(
  const fs::path& root,
  const std::string& repository,
  const std::string& branch,
  const std::string& explicitCommit,
  const std::string& explicitSha,
  bool allowReplaceUnmanaged
) {
  DependencyInstallResult result;

  const std::size_t slash = repository.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 >= repository.size() ||
      repository.find('/', slash + 1) != std::string::npos || !validBranchName(branch)) {
    result.error = "Expected owner/repository and a non-empty branch.";
    return result;
  }
  const std::string owner = repository.substr(0, slash);
  const std::string name = repository.substr(slash + 1);
  const auto validPart = [](const std::string& part) {
    return !part.empty() && std::all_of(part.begin(), part.end(), [](unsigned char c) {
      return std::isalnum(c) || c == '-' || c == '_' || c == '.';
    }) && part != "." && part != "..";
  };
  if (!validPart(owner) || !validPart(name)) {
    result.error = "Repository contains invalid path characters.";
    return result;
  }

  std::string commit = explicitCommit;
  if (commit.empty()) {
    std::string resolveError;
    if (!resolveBranchCommit(repository, branch, commit, resolveError)) {
      result.error = resolveError;
      return result;
    }
  }
  if (!isHex(commit, 40) && !isHex(commit, 64)) {
    result.error = "--rev must be a 40 or 64 character hexadecimal commit SHA.";
    return result;
  }
  if (!explicitSha.empty() && !isHex(explicitSha, 64)) {
    result.error = "--sha256 must be a 64 character hexadecimal digest.";
    return result;
  }

  std::error_code error;
  const fs::path external = root / "external";
  fs::create_directories(external, error);
  if (error) {
    result.error = "Could not create external/: " + error.message();
    return result;
  }

  const fs::path destination = external / name;
  const fs::path archive = external / ("." + name + ".download.zip");
  const fs::path stage = external / ("." + name + ".extract");
  const fs::path backup = external / ("." + name + ".backup");

  if (fs::exists(backup, error) || error) {
    result.error = "A previous installation backup exists at " + backup.string() + "; inspect it before continuing.";
    return result;
  }
  error.clear();
  fs::remove_all(stage, error);
  if (error) {
    result.error = "Could not clean the previous extraction directory: " + error.message();
    return result;
  }
  fs::create_directories(stage, error);
  if (error) {
    result.error = "Could not create temporary directory: " + error.message();
    return result;
  }

  const std::string url = "https://github.com/" + owner + "/" + name + "/archive/" + commit + ".zip";
#ifndef FRANKENSTEIN_HAS_CPR
  fs::remove(archive, error);
  fs::remove_all(stage, error);
  result.error = "This Frankenstein build has no CPR support.";
  return result;
#else
  std::ofstream archiveOutput(archive, std::ios::binary | std::ios::trunc);
  if (!archiveOutput) {
    fs::remove_all(stage, error);
    result.error = "Could not create ZIP file.";
    return result;
  }
  const cpr::Response response = cpr::Download(
    archiveOutput,
    cpr::Url{url},
    cpr::Redirect{true},
    cpr::VerifySsl{true},
    cpr::Timeout{120000}
  );
  archiveOutput.close();
  if (response.error || response.status_code != 200 || response.downloaded_bytes <= 0) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "GitHub ZIP download failed (HTTP " + std::to_string(response.status_code) + ").";
    return result;
  }
  if (response.downloaded_bytes > 100LL * 1024 * 1024) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "GitHub ZIP exceeds the 100 MiB download limit.";
    return result;
  }
#endif

  const std::string computedSha = sha256File(archive);
  if (!isHex(computedSha, 64)) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "Could not calculate the downloaded ZIP SHA-256.";
    return result;
  }
  if (!explicitSha.empty() && computedSha != explicitSha) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "Downloaded ZIP SHA-256 does not match the expected value.";
    return result;
  }

  const fs::path previous = fs::current_path();
  fs::current_path(stage, error);
  const int unpackStatus = error ? -1 : std::system(("cmake -E tar xf " + shellQuote(archive.string())).c_str());
  std::error_code restoreCwdError;
  fs::current_path(previous, restoreCwdError);
  if (unpackStatus != 0 || restoreCwdError) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "Could not extract the GitHub ZIP archive or restore the working directory.";
    return result;
  }

  fs::path extracted;
  for (const fs::directory_entry& entry : fs::directory_iterator(stage, error)) {
    if (entry.is_directory(error) && !error) {
      extracted = entry.path();
      break;
    }
  }
  if (error || extracted.empty() || !fs::is_regular_file(extracted / "CMakeLists.txt", error) || error) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "The downloaded repository has no usable root CMakeLists.txt.";
    return result;
  }

  {
    std::ofstream marker(extracted / ".frankenstein-dependency", std::ios::trunc);
    marker << repository << '\n' << branch << '\n' << commit << '\n' << computedSha << '\n';
    marker.close();
    if (!marker) {
      fs::remove(archive, error);
      fs::remove_all(stage, error);
      result.error = "Could not write dependency metadata.";
      return result;
    }
  }

  bool backedUp = false;
  if (fs::exists(destination, error)) {
    if (error || !fs::is_directory(destination, error) || error) {
      fs::remove(archive, error);
      fs::remove_all(stage, error);
      result.error = "The dependency destination exists but is not a directory.";
      return result;
    }
    std::ifstream oldMarker(destination / ".frankenstein-dependency");
    std::string oldRepository;
    const bool hasOldRepository = static_cast<bool>(std::getline(oldMarker, oldRepository));
    const std::size_t oldSlash = oldRepository.find('/');
    const bool oldPackageIsManaged = hasOldRepository && oldSlash != std::string::npos &&
        oldRepository.find('/', oldSlash + 1) == std::string::npos &&
        validPart(oldRepository.substr(0, oldSlash)) && validPart(oldRepository.substr(oldSlash + 1));
    if (!oldPackageIsManaged && !allowReplaceUnmanaged) {
      fs::remove(archive, error);
      fs::remove_all(stage, error);
      result.error = "Refusing to overwrite an unmanaged directory at external/" + name + ".";
      return result;
    }
    fs::rename(destination, backup, error);
    if (error) {
      fs::remove(archive, error);
      fs::remove_all(stage, error);
      result.error = "Could not stage the previous dependency before replacement: " + error.message();
      return result;
    }
    backedUp = true;
  } else if (error) {
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "Could not inspect the dependency destination.";
    return result;
  }

  error.clear();
  fs::rename(extracted, destination, error);
  if (error) {
    if (backedUp) {
      std::error_code rollbackError;
      fs::rename(backup, destination, rollbackError);
    }
    fs::remove(archive, error);
    fs::remove_all(stage, error);
    result.error = "Could not install dependency; previous installation was restored when possible.";
    return result;
  }

  fs::remove(archive, error);
  error.clear();
  fs::remove_all(stage, error);
  if (backedUp) {
    error.clear();
    fs::remove_all(backup, error);
    if (error) {
      result.error = "Dependency was installed, but the previous backup could not be removed: " + error.message();
      return result;
    }
  }

  result.success = true;
  result.commit = commit;
  result.sha256 = computedSha;
  return result;
}

}
