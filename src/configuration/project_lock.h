#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace configuration {

struct LockEntry {
  std::string repository;
  std::string branch;
  std::string commit;
  std::string sha256;
};

std::vector<LockEntry> readLock(const std::filesystem::path& path);
bool writeLock(const std::filesystem::path& path, const std::vector<LockEntry>& entries);
bool upsertLockEntry(const std::filesystem::path& path, const LockEntry& entry);
bool removeLockEntry(const std::filesystem::path& path, const std::string& repository);
bool hasLockEntry(const std::filesystem::path& path, const std::string& repository);

}