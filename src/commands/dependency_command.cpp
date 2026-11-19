#include "dependency_command.h"
#include "../cli/console.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cctype>
#include <iterator>
#include <vector>
#include <utility>
#include <array>
#include <cstdio>
#include <sstream>
#include <algorithm>

#ifdef FRANKENSTEIN_HAS_CPR
#include <cpr/cpr.h>
#endif

namespace {
std::string shellQuote(const std::string& value) {
#ifdef _WIN32
  std::string out = "\"";
  for (char c : value) { if (c == '"') out += '\\'; out += c; }
  return out + "\"";
#else
  std::string out = "'";
  for (char c : value) { if (c == '\'') out += "'\\''"; else out += c; }
  return out + "'";
#endif
}
bool validPart(const std::string& s) {
  if (s.empty() || s == "." || s == "..") return false;
  for (unsigned char c : s) if (!(std::isalnum(c) || c == '-' || c == '_' || c == '.')) return false;
  return true;
}
bool validBranch(const std::string& branch) {
  if (branch.empty()) return false;
  std::size_t start = 0;
  while (start <= branch.size()) {
    const auto end = branch.find('/', start);
    if (!validPart(branch.substr(start, end == std::string::npos ? end : end - start))) return false;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return true;
}
bool isHex(const std::string& value, std::size_t length) {
  if (value.size() != length) return false;
  for (unsigned char c : value) if (!std::isxdigit(c)) return false;
  return true;
}
std::string encodePathPart(const std::string& value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') result += static_cast<char>(c);
    else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
  }
  return result;
}
std::vector<std::array<std::string, 4>> readLock(const std::filesystem::path& path) {
  std::vector<std::array<std::string, 4>> records;
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string, 4> fields;
    std::size_t from = 0;
    bool valid = true;
    for (std::size_t i = 0; i < fields.size(); ++i) {
      const auto end = i + 1 == fields.size() ? std::string::npos : line.find('|', from);
      if (i + 1 != fields.size() && end == std::string::npos) { valid = false; break; }
      fields[i] = line.substr(from, end == std::string::npos ? end : end - from);
      from = end == std::string::npos ? line.size() : end + 1;
    }
    if (valid) records.push_back(std::move(fields));
  }
  return records;
}
bool writeLock(const std::filesystem::path& path, const std::array<std::string, 4>& record) {
  auto records = readLock(path);
  records.erase(std::remove_if(records.begin(), records.end(), [&](const auto& current) { return current[0] == record[0]; }), records.end());
  records.push_back(record);
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::trunc);
  if (!output) return false;
  output << "# Frankenstein dependency lock v1\n";
  for (const auto& current : records) output << current[0] << '|' << current[1] << '|' << current[2] << '|' << current[3] << '\n';
  output.close();
  if (!output) return false;
  std::error_code error;
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temporary, path, error);
  return !error;
}
void removeLockEntry(const std::filesystem::path& path, const std::string& repo) {
  auto records = readLock(path);
  records.erase(std::remove_if(records.begin(), records.end(), [&](const auto& current) { return current[0] == repo; }), records.end());
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::trunc);
  if (!output) return;
  output << "# Frankenstein dependency lock v1\n";
  for (const auto& current : records) output << current[0] << '|' << current[1] << '|' << current[2] << '|' << current[3] << '\n';
  output.close();
  if (!output) return;
  std::error_code error;
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temporary, path, error);
}
std::string sha256File(const std::filesystem::path& path) {
  const std::string command = "cmake -E sha256sum " + shellQuote(path.string());
#ifdef _WIN32
  FILE* pipe = _popen(command.c_str(), "r");
#else
  FILE* pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) return {};
  char buffer[256]{};
  std::string output;
  while (std::fgets(buffer, sizeof(buffer), pipe)) output += buffer;
#ifdef _WIN32
  const int result = _pclose(pipe);
#else
  const int result = pclose(pipe);
#endif
  if (result != 0) return {};
  std::istringstream stream(output);
  std::string digest;
  stream >> digest;
  return digest;
}
bool resolveCommit(const std::string& owner, const std::string& name, const std::string& branch, std::string& commit, std::string& error_message) {
#ifdef FRANKENSTEIN_HAS_CPR
  const std::string url = "https://api.github.com/repos/" + owner + "/" + name + "/commits/" + encodePathPart(branch);
  const auto response = cpr::Get(cpr::Url{url}, cpr::Header{{"Accept", "application/vnd.github+json"}, {"User-Agent", "frankenstein"}}, cpr::Redirect{true}, cpr::VerifySsl{true}, cpr::Timeout{30000});
  if (response.error || response.status_code != 200) {
    error_message = "Could not resolve GitHub branch (HTTP " + std::to_string(response.status_code) + ").";
    return false;
  }
  const auto key = response.text.find("\"sha\"");
  const auto colon = key == std::string::npos ? key : response.text.find(':', key);
  const auto first = colon == std::string::npos ? colon : response.text.find('"', colon);
  const auto last = first == std::string::npos ? first : response.text.find('"', first + 1);
  if (last == std::string::npos) { error_message = "GitHub returned no commit SHA."; return false; }
  commit = response.text.substr(first + 1, last - first - 1);
  if (!isHex(commit, 40) && !isHex(commit, 64)) { error_message = "GitHub returned an invalid commit SHA."; return false; }
  return true;
#else
  (void)owner; (void)name; (void)branch; (void)commit;
  error_message = "This Frankenstein build has no CPR support.";
  return false;
#endif
}
std::vector<std::pair<std::string, std::string>> readDependencies(const std::filesystem::path& manifest) {
  std::vector<std::pair<std::string, std::string>> result;
  std::ifstream input(manifest);
  std::string line;
  bool in_dependencies = false;
  while (std::getline(input, line)) {
    if (line == "[dependencies]") { in_dependencies = true; continue; }
    if (!line.empty() && line.front() == '[') { in_dependencies = false; continue; }
    if (!in_dependencies) continue;
    const auto first = line.find('"');
    const auto middle = first == std::string::npos ? first : line.find('"', first + 1);
    const auto value = middle == std::string::npos ? middle : line.find('"', middle + 1);
    const auto last = value == std::string::npos ? value : line.find('"', value + 1);
    if (first != std::string::npos && middle != std::string::npos && value != std::string::npos && last != std::string::npos)
      result.emplace_back(line.substr(first + 1, middle - first - 1), line.substr(value + 1, last - value - 1));
  }
  return result;
}
int executeNested(DependencyCommand::Action action, const std::vector<std::string>& arguments) {
  std::vector<std::string> storage = {"frankenstein", action == DependencyCommand::Action::Add ? "add" : "remove"};
  storage.insert(storage.end(), arguments.begin(), arguments.end());
  std::vector<char*> argv;
  for (auto& item : storage) argv.push_back(item.data());
  CLI cli;
  if (!cli.parse(static_cast<int>(argv.size()), argv.data())) return 1;
  return DependencyCommand(action).execute(cli);
}
}

int DependencyCommand::execute(const CLI& cli) {
  namespace fs = std::filesystem;
  if (action == Action::List || action == Action::Install || action == Action::Update) {
    const auto dependencies = readDependencies("Frankenstein.toml");
    if (action == Action::List) {
      if (dependencies.empty()) { Console::print("No dependencies."); return 0; }
      for (const auto& [repo, branch] : dependencies) Console::print("  " + repo + " (" + branch + ")");
      return 0;
    }
    for (const auto& [repo, branch] : dependencies) {
      const auto slash = repo.find('/');
      const fs::path installed = slash == std::string::npos ? fs::path{} : fs::path("external") / repo.substr(slash + 1);
      const auto records = readLock("Frankenstein.lock");
      const auto lock = std::find_if(records.begin(), records.end(), [&](const auto& item) { return item[0] == repo; });
      if (action == Action::Install && lock == records.end()) {
        Console::error("No locked revision for " + repo + ". Add it again to generate Frankenstein.lock.");
        return 1;
      }
      if (action == Action::Install && fs::exists(installed)) {
        std::ifstream marker(installed / ".frankenstein-dependency");
        std::string installed_repo, installed_branch, installed_commit, installed_sha;
        std::getline(marker, installed_repo);
        std::getline(marker, installed_branch);
        std::getline(marker, installed_commit);
        std::getline(marker, installed_sha);
        if (installed_repo == repo && installed_branch == branch && installed_commit == (*lock)[2] && installed_sha == (*lock)[3]) continue;
        Console::error("Installed files for " + repo + " do not match Frankenstein.lock; remove the dependency before installing.");
        return 1;
      }
      if (action == Action::Update && fs::exists(installed)) {
        if (executeNested(Action::Remove, {repo}) != 0) return 1;
      }
      if (action == Action::Install) {
        if (executeNested(Action::Add, {repo + ":" + branch, "--rev", (*lock)[2], "--sha256", (*lock)[3]}) != 0) return 1;
      } else if (executeNested(Action::Add, {repo + ":" + branch}) != 0) return 1;
    }
    Console::success(action == Action::Install ? "Dependencies installed." : "Dependencies updated.");
    return 0;
  }
  if (cli.getArguments().empty()) {
    Console::error(action == Action::Add ? "Usage: frankenstein add owner/repository[:branch]" : "Usage: frankenstein remove owner/repository");
    return 1;
  }
  std::string repo = cli.getArguments()[0];
  std::string branch = "main";
  const std::string requested_commit = cli.hasFlag("--rev") ? cli.getFlagValue("--rev") : std::string{};
  const std::string expected_archive_sha = cli.hasFlag("--sha256") ? cli.getFlagValue("--sha256") : std::string{};
  if (cli.hasFlag("--branch")) branch = cli.getFlagValue("--branch");
  const auto colon = repo.find(':');
  if (colon != std::string::npos) {
    if (cli.hasFlag("--branch")) { Console::error("Use either :branch or --branch, not both."); return 1; }
    branch = repo.substr(colon + 1); repo.resize(colon);
  }
  const auto slash = repo.find('/');
  if (slash == std::string::npos || repo.find('/', slash + 1) != std::string::npos || !validPart(repo.substr(0, slash)) || !validPart(repo.substr(slash + 1)) || !validBranch(branch)) {
    Console::error("Expected owner/repository[:branch] with safe path characters."); return 1;
  }
  const std::string owner = repo.substr(0, slash);
  const std::string name = repo.substr(slash + 1);
  const fs::path root = fs::current_path();
  const fs::path external = root / "external";
  const fs::path destination = external / name;
  std::error_code ec;

  if (action == Action::Remove) {
    if (!fs::exists(destination, ec) || ec) { Console::error("Dependency is not installed: " + repo); return 1; }
    const fs::path marker = destination / ".frankenstein-dependency";
    std::ifstream marker_file(marker);
    std::string recorded_repo;
    std::getline(marker_file, recorded_repo);
    if (!marker_file && recorded_repo.empty()) { Console::error("Refusing to remove an unmanaged external/ directory: " + destination.string()); return 1; }
    if (recorded_repo != repo) { Console::error("Dependency metadata does not match " + repo + "; refusing removal."); return 1; }
    const fs::path cmake_file = root / "CMakeLists.txt";
    std::ifstream in(cmake_file);
    std::string content((std::istreambuf_iterator<char>(in)), {});
    in.close();
    std::string recorded_branch;
    std::getline(marker_file, recorded_branch);
    marker_file.close();
    const std::string begin = "# BEGIN FRANKENSTEIN DEPENDENCY " + name;
    const std::string end = "# END FRANKENSTEIN DEPENDENCY " + name;
    auto a = content.find(begin);
    auto b = a == std::string::npos ? a : content.find(end, a);
    if (a != std::string::npos && b != std::string::npos) {
      b = content.find('\n', b);
      content.erase(a, b == std::string::npos ? std::string::npos : b - a + 1);
      std::ofstream out(cmake_file, std::ios::trunc); out << content;
      if (!out) { Console::error("Could not update CMakeLists.txt."); return 1; }
    }
    fs::remove_all(destination, ec);
    if (ec) { Console::error("Could not remove dependency: " + ec.message()); return 1; }
    const fs::path manifest = root / "Frankenstein.toml";
    if (fs::exists(manifest)) {
      std::ifstream manifest_in(manifest);
      std::string text((std::istreambuf_iterator<char>(manifest_in)), {});
      manifest_in.close();
      const std::string line = "\"" + repo + "\" = \"" + recorded_branch + "\"\n";
      const auto at = text.find(line);
      if (at != std::string::npos) { text.erase(at, line.size()); std::ofstream manifest_out(manifest, std::ios::trunc); manifest_out << text; }
    }
    removeLockEntry(root / "Frankenstein.lock", repo);
    Console::success("Removed " + repo); return 0;
  }

  if (!fs::exists(root / "CMakeLists.txt", ec) || ec) { Console::error("Run this command in a project containing CMakeLists.txt."); return 1; }
  if (fs::exists(destination, ec)) { Console::error("Destination already exists; refusing to overwrite: " + destination.string()); return 1; }
#ifndef FRANKENSTEIN_HAS_CPR
  Console::error("This Frankenstein build has no CPR support. Enable the bundled CPR target in CMake.");
  return 1;
#else
  fs::create_directories(external, ec);
  if (ec) { Console::error("Could not create external/: " + ec.message()); return 1; }
  const fs::path archive = external / ("." + name + ".download.zip");
  const fs::path stage = external / ("." + name + ".extract");
  fs::create_directories(stage, ec);
  if (ec) { Console::error("Could not create temporary directory: " + ec.message()); return 1; }
  std::string commit = requested_commit;
  std::string resolve_error;
  if (commit.empty() && !resolveCommit(owner, name, branch, commit, resolve_error)) {
    fs::remove_all(stage, ec);
    Console::error(resolve_error);
    return 1;
  }
  if (!isHex(commit, 40) && !isHex(commit, 64)) {
    fs::remove_all(stage, ec);
    Console::error("--rev must be a 40 or 64 character hexadecimal commit SHA.");
    return 1;
  }
  if (!expected_archive_sha.empty() && !isHex(expected_archive_sha, 64)) {
    fs::remove_all(stage, ec);
    Console::error("--sha256 must be a 64 character hexadecimal digest.");
    return 1;
  }
  const std::string url = "https://github.com/" + owner + "/" + name + "/archive/" + commit + ".zip";
  std::ofstream output(archive, std::ios::binary | std::ios::trunc);
  if (!output) { fs::remove_all(stage, ec); Console::error("Could not create ZIP file."); return 1; }
  cpr::Response response = cpr::Download(output, cpr::Url{url}, cpr::Redirect{true}, cpr::VerifySsl{true}, cpr::Timeout{120000});
  output.close();
  if (response.error || response.status_code != 200 || response.downloaded_bytes <= 0) {
    fs::remove(archive, ec); fs::remove_all(stage, ec);
    Console::error("GitHub ZIP download failed (HTTP " + std::to_string(response.status_code) + "): " + response.error.message);
    return 1;
  }
  if (response.downloaded_bytes > 100LL * 1024 * 1024) {
    fs::remove(archive, ec); fs::remove_all(stage, ec);
    Console::error("GitHub ZIP exceeds the 100 MiB download limit."); return 1;
  }
  const std::string archive_sha = sha256File(archive);
  if (!isHex(archive_sha, 64)) {
    fs::remove(archive, ec); fs::remove_all(stage, ec);
    Console::error("Could not calculate the downloaded ZIP SHA-256."); return 1;
  }
  if (!expected_archive_sha.empty() && archive_sha != expected_archive_sha) {
    fs::remove(archive, ec); fs::remove_all(stage, ec);
    Console::error("Downloaded ZIP SHA-256 does not match Frankenstein.lock."); return 1;
  }
  const std::string unpack = "cmake -E tar xf " + shellQuote(archive.string());
  const fs::path old = fs::current_path();
  fs::current_path(stage, ec);
  int result = ec ? -1 : std::system(unpack.c_str());
  fs::current_path(old, ec);
  if (result != 0) { fs::remove(archive, ec); fs::remove_all(stage, ec); Console::error("Could not extract the GitHub ZIP archive."); return 1; }
  fs::path source;
  for (const auto& entry : fs::directory_iterator(stage, ec)) if (entry.is_directory()) { source = entry.path(); break; }
  if (ec || source.empty()) { fs::remove(archive, ec); fs::remove_all(stage, ec); Console::error("The ZIP has no repository root directory."); return 1; }
  fs::rename(source, destination, ec);
  if (ec) { fs::remove(archive, ec); fs::remove_all(stage, ec); Console::error("Could not install dependency: " + ec.message()); return 1; }
  fs::remove(archive, ec); fs::remove_all(stage, ec);

  {
    std::ofstream marker(destination / ".frankenstein-dependency", std::ios::trunc);
    marker << repo << '\n' << branch << '\n' << commit << '\n' << archive_sha << '\n';
    if (!marker) { fs::remove_all(destination, ec); Console::error("Could not write dependency metadata."); return 1; }
  }
  if (!writeLock(root / "Frankenstein.lock", {repo, branch, commit, archive_sha})) {
    fs::remove_all(destination, ec);
    Console::error("Could not update Frankenstein.lock.");
    return 1;
  }
  const fs::path manifest = root / "Frankenstein.toml";
  if (fs::exists(manifest)) {
    std::ifstream manifest_in(manifest);
    std::string text((std::istreambuf_iterator<char>(manifest_in)), {});
    manifest_in.close();
    if (text.find("[dependencies]") == std::string::npos) text += "\n[dependencies]\n";
    text += "\"" + repo + "\" = \"" + branch + "\"\n";
    std::ofstream manifest_out(manifest, std::ios::trunc); manifest_out << text;
    if (!manifest_out) { fs::remove_all(destination, ec); removeLockEntry(root / "Frankenstein.lock", repo); Console::error("Could not update Frankenstein.toml."); return 1; }
  }

  if (fs::exists(destination / "CMakeLists.txt")) {
    std::ofstream cmake(root / "CMakeLists.txt", std::ios::app);
    cmake << "\n# BEGIN FRANKENSTEIN DEPENDENCY " << name << "\n";
    if (name == "cpr") {
      cmake << "set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL \"\" FORCE)\n"
          << "set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL \"\" FORCE)\n"
          << "set(CPR_CURL_USE_NGHTTP2 OFF CACHE BOOL \"\" FORCE)\n"
          << "set(CPR_ENABLE_INSTALL OFF CACHE BOOL \"\" FORCE)\n"
          << "set(CPR_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n";
    }
    cmake << "add_subdirectory(external/" << name << " EXCLUDE_FROM_ALL)\n"
        << "if(TARGET " << name << "::" << name << ")\n"
        << "  target_link_libraries(${PROJECT_NAME} PRIVATE " << name << "::" << name << ")\n"
        << "elseif(TARGET " << name << ")\n"
        << "  target_link_libraries(${PROJECT_NAME} PRIVATE " << name << ")\n"
        << "else()\n"
        << "  get_filename_component(_frankenstein_dep_dir \"${CMAKE_CURRENT_SOURCE_DIR}/external/" << name << "\" ABSOLUTE)\n"
        << "  get_property(_frankenstein_dep_targets DIRECTORY \"${_frankenstein_dep_dir}\" PROPERTY BUILDSYSTEM_TARGETS)\n"
        << "  set(_frankenstein_linkable_targets)\n"
        << "  foreach(_frankenstein_target IN LISTS _frankenstein_dep_targets)\n"
        << "    get_target_property(_frankenstein_type ${_frankenstein_target} TYPE)\n"
        << "    if(_frankenstein_type MATCHES \"_LIBRARY$\")\n"
        << "      list(APPEND _frankenstein_linkable_targets ${_frankenstein_target})\n"
        << "    endif()\n"
        << "  endforeach()\n"
        << "  list(LENGTH _frankenstein_linkable_targets _frankenstein_linkable_count)\n"
        << "  if(_frankenstein_linkable_count EQUAL 1)\n"
        << "    list(GET _frankenstein_linkable_targets 0 _frankenstein_linkable_target)\n"
        << "    target_link_libraries(${PROJECT_NAME} PRIVATE ${_frankenstein_linkable_target})\n"
        << "  else()\n"
        << "    message(STATUS \"Frankenstein: no unique library target detected for " << name << "; inspect external/" << name << " manually.\")\n"
        << "  endif()\n"
        << "endif()\n";
    cmake << "# END FRANKENSTEIN DEPENDENCY " << name << "\n";
    if (!cmake) { Console::error("Library downloaded, but CMakeLists.txt could not be updated."); return 1; }
  } else {
    Console::warning("No CMakeLists.txt found; source is saved in external/" + name + " for manual integration.");
  }
  Console::success("Added " + repo + " to external/" + name);
  return 0;
#endif
}
