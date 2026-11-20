#include "dependency_command.h"
#include "../cli/console.h"
#include "../cli/prompt.h"
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
#include <set>

#ifdef FRANKENSTEIN_HAS_CPR
#include <cpr/cpr.h>
#endif

namespace {
struct Dependency {
  std::string name;
  std::string repository;
  std::string branch;
  std::string target;
};

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

bool writeLockRecords(const std::filesystem::path& path, const std::vector<std::array<std::string, 4>>& records) {
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::trunc);
  if (!output) return false;
  output << "# Frankenstein dependency lock v1\n";
  for (const auto& r : records) output << r[0] << '|' << r[1] << '|' << r[2] << '|' << r[3] << '\n';
  output.close();
  if (!output) return false;
  std::error_code error;
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temporary, path, error);
  return !error;
}

bool writeLock(const std::filesystem::path& path, const std::array<std::string, 4>& record) {
  auto records = readLock(path);
  records.erase(std::remove_if(records.begin(), records.end(), [&](const auto& current) { return current[0] == record[0]; }), records.end());
  records.push_back(record);
  return writeLockRecords(path, records);
}

void removeLockEntry(const std::filesystem::path& path, const std::string& repo) {
  auto records = readLock(path);
  records.erase(std::remove_if(records.begin(), records.end(), [&](const auto& current) { return current[0] == repo; }), records.end());
  writeLockRecords(path, records);
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

std::vector<Dependency> readDependencies(const std::filesystem::path& manifest) {
  std::vector<Dependency> result;
  std::ifstream input(manifest);
  if (!input) return result;
  std::string line;
  Dependency current;
  bool in_table = false;
  auto flush = [&]() {
    if (in_table && !current.name.empty()) result.push_back(current);
    current = Dependency{};
    in_table = false;
  };
  while (std::getline(input, line)) {
    const auto stripped = trim(line);
    if (stripped.empty() || stripped.front() == '#') continue;
    if (stripped.front() == '[') {
      flush();
      const auto close = stripped.find(']');
      if (close == std::string::npos) continue;
      const auto header = stripped.substr(1, close - 1);
      const std::string prefix = "dependencies.";
      if (header.rfind(prefix, 0) == 0) {
        current.name = unquote(trim(header.substr(prefix.size())));
        in_table = !current.name.empty();
      }
      continue;
    }
    if (!in_table) continue;
    const auto eq = stripped.find('=');
    if (eq == std::string::npos) continue;
    const auto key = trim(stripped.substr(0, eq));
    const auto value = unquote(trim(stripped.substr(eq + 1)));
    if (key == "repository") current.repository = value;
    else if (key == "branch") current.branch = value;
    else if (key == "target") current.target = value;
  }
  flush();
  return result;
}

bool writeManifest(const std::filesystem::path& path, const std::vector<Dependency>& dependencies) {
  const auto temporary = path.string() + ".tmp";
  std::ofstream out(temporary, std::ios::trunc);
  if (!out) return false;
  out << "# Frankenstein dependency manifest\n";
  for (const auto& dep : dependencies) {
    out << "\n[dependencies." << dep.name << "]\n";
    out << "repository = \"" << dep.repository << "\"\n";
    out << "branch = \"" << dep.branch << "\"\n";
    if (!dep.target.empty()) out << "target = \"" << dep.target << "\"\n";
  }
  out.close();
  if (!out) return false;
  std::error_code ec;
  std::filesystem::remove(path, ec);
  ec.clear();
  std::filesystem::rename(temporary, path, ec);
  return !ec;
}

bool upsertDependency(const std::filesystem::path& manifest, const Dependency& dependency) {
  auto deps = readDependencies(manifest);
  auto it = std::find_if(deps.begin(), deps.end(), [&](const auto& d) {
    return d.name == dependency.name || d.repository == dependency.repository;
  });
  if (it != deps.end()) *it = dependency;
  else deps.push_back(dependency);
  return writeManifest(manifest, deps);
}

bool removeDependencyEntry(const std::filesystem::path& manifest, const std::string& repo) {
  auto deps = readDependencies(manifest);
  const auto before = deps.size();
  deps.erase(std::remove_if(deps.begin(), deps.end(), [&](const auto& d) { return d.repository == repo; }), deps.end());
  if (deps.size() == before) return true;
  return writeManifest(manifest, deps);
}

std::string cmakeString(const std::string& value) {
  std::string out;
  for (char c : value) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

bool detectTargets(const std::filesystem::path& project_root, const std::string& dependency_name, std::vector<std::string>& targets, std::string& error_message) {
  const std::filesystem::path scratch = project_root / "external" / (".frankenstein-targets-" + dependency_name);
  std::error_code ec;
  std::filesystem::remove_all(scratch, ec);
  std::filesystem::create_directories(scratch, ec);
  if (ec) { error_message = "Could not create target detection directory: " + ec.message(); return false; }

  const std::filesystem::path probe_dir = scratch / "probe";
  std::filesystem::create_directories(probe_dir, ec);
  if (ec) { std::filesystem::remove_all(scratch, ec); error_message = ec.message(); return false; }

  const std::filesystem::path source_dir = project_root / "external" / dependency_name;
  {
    std::ofstream cmake(probe_dir / "CMakeLists.txt", std::ios::trunc);
    if (!cmake) { std::filesystem::remove_all(scratch, ec); error_message = "Could not write detection CMakeLists.txt."; return false; }
    cmake << "cmake_minimum_required(VERSION 3.16)\n"
          << "project(frankenstein_target_probe LANGUAGES C CXX)\n"
          << "set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)\n"
          << "add_subdirectory(\"" << cmakeString(source_dir.string()) << "\" \"${CMAKE_BINARY_DIR}/dep\")\n"
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
          << "    file(APPEND \"${CMAKE_BINARY_DIR}/frankenstein_targets.txt\" \"${_t}\\n\")\n"
          << "  endif()\n"
          << "endforeach()\n";
  }

  const std::filesystem::path build_dir = scratch / "build";
  std::filesystem::create_directories(build_dir, ec);
  if (ec) { std::filesystem::remove_all(scratch, ec); error_message = ec.message(); return false; }

  const std::string configure =
    "cmake -S " + shellQuote(probe_dir.string()) +
    " -B " + shellQuote(build_dir.string()) +
    " -DCMAKE_BUILD_TYPE=Release";

  const std::filesystem::path log = scratch / "configure.log";
  const std::string command = configure + " > " + shellQuote(log.string()) + " 2>&1";
  const int status = std::system(command.c_str());
  if (status != 0) {
    std::ifstream log_in(log);
    std::ostringstream text;
    text << log_in.rdbuf();
    std::filesystem::remove_all(scratch, ec);
    error_message = "CMake failed to configure the dependency:\n" + text.str();
    return false;
  }

  std::ifstream list_file(build_dir / "frankenstein_targets.txt");
  if (!list_file) {
    std::filesystem::remove_all(scratch, ec);
    error_message = "CMake did not produce the target list.";
    return false;
  }

  std::set<std::string> unique;
  std::string line;
  while (std::getline(list_file, line)) {
    const auto cleaned = trim(line);
    if (!cleaned.empty()) unique.insert(cleaned);
  }
  targets.assign(unique.begin(), unique.end());
  std::filesystem::remove_all(scratch, ec);
  return true;
}

bool chooseTarget(const std::string& repo, const std::vector<std::string>& candidates, const std::string& dependency_name, std::string& target) {
  if (candidates.empty()) {
    Console::warning("No linkable CMake library target was found for " + repo + ".");
    Console::print("Set target manually in Frankenstein.toml under [dependencies." + dependency_name + "].");
    return false;
  }
  if (candidates.size() == 1) {
    target = candidates.front();
    Console::print("Detected CMake target: " + target);
    return true;
  }
  std::vector<std::string> options = candidates;
  options.push_back("Enter target name manually");
  const std::size_t index = prompt::select("Select CMake target for " + repo, options, 0);
  if (index < candidates.size()) {
    target = options[index];
    Console::print("Selected CMake target: " + target);
    return true;
  }
  const std::string manual = prompt::input("CMake target name", "");
  if (manual.empty()) {
    Console::error("No target selected.");
    return false;
  }
  target = manual;
  return true;
}

void appendCmakeBlock(const std::filesystem::path& cmake_file, const std::string& dependency_name, const std::string& target) {
  std::ofstream out(cmake_file, std::ios::app);
  if (!out) return;
  out << "\n# BEGIN FRANKENSTEIN DEPENDENCY " << dependency_name << "\n"
      << "add_subdirectory(external/" << dependency_name << " EXCLUDE_FROM_ALL)\n"
      << "if(NOT TARGET " << target << ")\n"
      << "  message(FATAL_ERROR \"Frankenstein: expected CMake target '" << target << "' from external/" << dependency_name << "'.\")\n"
      << "endif()\n"
      << "target_link_libraries(${PROJECT_NAME} PRIVATE " << target << ")\n"
      << "# END FRANKENSTEIN DEPENDENCY " << dependency_name << "\n";
}

bool removeCmakeBlock(const std::filesystem::path& cmake_file, const std::string& dependency_name) {
  std::ifstream in(cmake_file);
  if (!in) return false;
  std::string content((std::istreambuf_iterator<char>(in)), {});
  in.close();
  const std::string begin = "# BEGIN FRANKENSTEIN DEPENDENCY " + dependency_name;
  const std::string end = "# END FRANKENSTEIN DEPENDENCY " + dependency_name;
  auto a = content.find(begin);
  auto b = a == std::string::npos ? a : content.find(end, a);
  if (a == std::string::npos || b == std::string::npos) return true;
  b = content.find('\n', b);
  content.erase(a, b == std::string::npos ? std::string::npos : b - a + 1);
  std::ofstream out(cmake_file, std::ios::trunc);
  out << content;
  return static_cast<bool>(out);
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
  const fs::path root = fs::current_path();
  const fs::path manifest_path = root / "Frankenstein.toml";
  const fs::path lock_path = root / "Frankenstein.lock";
  const fs::path external = root / "external";
  std::error_code ec;

  if (action == Action::List) {
    const auto dependencies = readDependencies(manifest_path);
    if (dependencies.empty()) { Console::print("No dependencies."); return 0; }
    for (const auto& dep : dependencies) {
      Console::print("  " + dep.name + "  " + dep.repository + " (" + dep.branch + ")  target=" + (dep.target.empty() ? "<unset>" : dep.target));
    }
    return 0;
  }

  if (action == Action::Install || action == Action::Update) {
    const auto dependencies = readDependencies(manifest_path);
    if (dependencies.empty()) { Console::print("No dependencies."); return 0; }
    for (const auto& dep : dependencies) {
      const auto slash = dep.repository.find('/');
      const std::string dep_name = slash == std::string::npos ? dep.repository : dep.repository.substr(slash + 1);
      const fs::path installed = external / dep_name;
      const auto records = readLock(lock_path);
      const auto lock = std::find_if(records.begin(), records.end(), [&](const auto& item) { return item[0] == dep.repository; });

      if (action == Action::Install && lock == records.end()) {
        Console::error("No locked revision for " + dep.repository + ". Run 'frankenstein add' again.");
        return 1;
      }
      if (action == Action::Install && fs::exists(installed)) {
        std::ifstream marker(installed / ".frankenstein-dependency");
        std::string installed_repo, installed_branch, installed_commit, installed_sha;
        std::getline(marker, installed_repo);
        std::getline(marker, installed_branch);
        std::getline(marker, installed_commit);
        std::getline(marker, installed_sha);
        if (installed_repo == dep.repository && installed_branch == dep.branch && installed_commit == (*lock)[2] && installed_sha == (*lock)[3]) continue;
        Console::error("Installed files for " + dep.repository + " do not match Frankenstein.lock.");
        return 1;
      }
      if (action == Action::Update && fs::exists(installed)) {
        if (executeNested(Action::Remove, {dep.repository}) != 0) return 1;
      }
      if (action == Action::Install) {
        if (executeNested(Action::Add, {dep.repository + ":" + dep.branch, "--rev", (*lock)[2], "--sha256", (*lock)[3]}) != 0) return 1;
      } else {
        if (executeNested(Action::Add, {dep.repository + ":" + dep.branch}) != 0) return 1;
      }
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
  const fs::path destination = external / name;

  if (action == Action::Remove) {
    if (!fs::exists(destination, ec) || ec) { Console::error("Dependency is not installed: " + repo); return 1; }
    const fs::path marker = destination / ".frankenstein-dependency";
    std::ifstream marker_file(marker);
    std::string recorded_repo;
    std::getline(marker_file, recorded_repo);
    if (!marker_file && recorded_repo.empty()) { Console::error("Refusing to remove an unmanaged external/ directory: " + destination.string()); return 1; }
    if (recorded_repo != repo) { Console::error("Dependency metadata does not match " + repo + "; refusing removal."); return 1; }
    marker_file.close();
    const fs::path cmake_file = root / "CMakeLists.txt";
    if (fs::exists(cmake_file)) {
      if (!removeCmakeBlock(cmake_file, name)) { Console::error("Could not update CMakeLists.txt."); return 1; }
    }
    fs::remove_all(destination, ec);
    if (ec) { Console::error("Could not remove dependency: " + ec.message()); return 1; }
    if (fs::exists(manifest_path)) removeDependencyEntry(manifest_path, repo);
    removeLockEntry(lock_path, repo);
    Console::success("Removed " + repo);
    return 0;
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

  if (!fs::exists(destination / "CMakeLists.txt")) {
    fs::remove_all(destination, ec);
    Console::error("Dependency does not ship a CMakeLists.txt; cannot integrate with CMake.");
    return 1;
  }

  std::vector<std::string> candidates;
  std::string detect_error;
  if (!detectTargets(root, name, candidates, detect_error)) {
    fs::remove_all(destination, ec);
    Console::error("Target detection failed for " + repo + ".\n" + detect_error);
    return 1;
  }

  std::string target;
  if (!chooseTarget(repo, candidates, name, target)) {
    fs::remove_all(destination, ec);
    Console::error("Aborting add: no CMake target selected.");
    return 1;
  }

  Dependency dependency{name, repo, branch, target};
  if (!upsertDependency(manifest_path, dependency)) {
    fs::remove_all(destination, ec);
    Console::error("Could not update Frankenstein.toml.");
    return 1;
  }

  if (!writeLock(lock_path, {repo, branch, commit, archive_sha})) {
    fs::remove_all(destination, ec);
    Console::error("Could not update Frankenstein.lock.");
    return 1;
  }

  appendCmakeBlock(root / "CMakeLists.txt", name, target);

  Console::success("Added " + repo + " to external/" + name + " (target: " + target + ")");
  return 0;
#endif
}