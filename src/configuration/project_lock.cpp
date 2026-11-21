#include "project_lock.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace configuration {

namespace {

std::vector<std::string> split(const std::string& line, char delimiter) {
  std::vector<std::string> parts;
  std::istringstream stream(line);
  std::string part;
  while (std::getline(stream, part, delimiter)) {
    parts.push_back(part);
  }
  return parts;
}

}

std::vector<LockEntry> readLock(const std::filesystem::path& path) {
  std::vector<LockEntry> entries;
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const std::vector<std::string> parts = split(line, '|');
    if (parts.size() != 4) {
      continue;
    }
    entries.push_back(LockEntry{parts[0], parts[1], parts[2], parts[3]});
  }
  return entries;
}

bool writeLock(const std::filesystem::path& path, const std::vector<LockEntry>& entries) {
  const std::filesystem::path temporary = path.string() + ".tmp";
  const std::filesystem::path backup = path.string() + ".bak";
  std::ofstream out(temporary, std::ios::trunc);
  if (!out) return false;
  out << "# Frankenstein.lock\n";
  out << "# repository|branch|commit|sha256\n";
  for (const LockEntry& entry : entries) {
    out << entry.repository << '|'
        << entry.branch << '|'
        << entry.commit << '|'
        << entry.sha256 << '\n';
  }
  out.close();
  if (!out) {
    std::error_code cleanupError;
    std::filesystem::remove(temporary, cleanupError);
    return false;
  }

  std::error_code error;
  const bool hadOriginal = std::filesystem::exists(path, error);
  if (error) return false;
  if (std::filesystem::exists(backup, error) || error) return false;
  if (hadOriginal) {
    std::filesystem::rename(path, backup, error);
    if (error) return false;
  }

  std::filesystem::rename(temporary, path, error);
  if (error) {
    if (hadOriginal) {
      std::error_code restoreError;
      std::filesystem::rename(backup, path, restoreError);
    }
    return false;
  }

  if (hadOriginal) {
    std::error_code cleanupError;
    std::filesystem::remove(backup, cleanupError);
    if (cleanupError) return false;
  }
  return true;
}

bool upsertLockEntry(const std::filesystem::path& path, const LockEntry& entry) {
  std::vector<LockEntry> entries = readLock(path);
  auto iterator = std::find_if(
    entries.begin(),
    entries.end(),
    [&](const LockEntry& current) { return current.repository == entry.repository; }
  );
  if (iterator == entries.end()) {
    entries.push_back(entry);
  } else {
    *iterator = entry;
  }
  return writeLock(path, entries);
}

bool removeLockEntry(const std::filesystem::path& path, const std::string& repository) {
  std::vector<LockEntry> entries = readLock(path);
  const std::size_t before = entries.size();
  entries.erase(
    std::remove_if(
      entries.begin(),
      entries.end(),
      [&](const LockEntry& current) { return current.repository == repository; }
    ),
    entries.end()
  );
  if (entries.size() == before) {
    return true;
  }
  return writeLock(path, entries);
}

bool hasLockEntry(const std::filesystem::path& path, const std::string& repository) {
  const std::vector<LockEntry> entries = readLock(path);
  return std::any_of(
    entries.begin(),
    entries.end(),
    [&](const LockEntry& current) { return current.repository == repository; }
  );
}

}