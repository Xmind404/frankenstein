#include "search_command.h"

#include "../cli/ascii.h"
#include "../cli/console.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#ifdef FRANKENSTEIN_HAS_CPR
#include <cpr/cpr.h>
#endif

namespace {

std::string urlEncode(const std::string& value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else if (c == ' ') {
      result += '+';
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}

std::string jsonString(const std::string& object, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  std::size_t position = object.find(token);
  if (position == std::string::npos) return {};
  position = object.find(':', position + token.size());
  if (position == std::string::npos) return {};
  ++position;
  while (position < object.size() && std::isspace(static_cast<unsigned char>(object[position]))) ++position;
  if (position >= object.size() || object[position] != '"') return {};
  ++position;

  std::string result;
  while (position < object.size()) {
    const char c = object[position++];
    if (c == '"') return result;
    if (c != '\\') {
      result += c;
      continue;
    }
    if (position >= object.size()) return {};
    const char escaped = object[position++];
    switch (escaped) {
      case '"': result += '"'; break;
      case '\\': result += '\\'; break;
      case '/': result += '/'; break;
      case 'b': result += '\b'; break;
      case 'f': result += '\f'; break;
      case 'n': result += '\n'; break;
      case 'r': result += '\r'; break;
      case 't': result += '\t'; break;
      case 'u':
        // Keep unsupported Unicode escape sequences visible rather than
        // accidentally consuming arbitrary bytes from the JSON document.
        if (position + 4 <= object.size()) {
          result += "\\u";
          result.append(object, position, 4);
          position += 4;
        }
        break;
      default: result += escaped; break;
    }
  }
  return {};
}

long long jsonNumber(const std::string& object, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  std::size_t position = object.find(token);
  if (position == std::string::npos) return 0;
  position = object.find(':', position + token.size());
  if (position == std::string::npos) return 0;
  ++position;
  while (position < object.size() && std::isspace(static_cast<unsigned char>(object[position]))) ++position;
  std::size_t end = position;
  while (end < object.size() && std::isdigit(static_cast<unsigned char>(object[end]))) ++end;
  if (end == position) return 0;
  try { return std::stoll(object.substr(position, end - position)); }
  catch (...) { return 0; }
}

std::vector<std::string> jsonObjects(const std::string& json) {
  std::vector<std::string> objects;
  const std::size_t itemsKey = json.find("\"items\"");
  if (itemsKey == std::string::npos) return objects;
  const std::size_t arrayStart = json.find('[', itemsKey);
  if (arrayStart == std::string::npos) return objects;

  bool inString = false;
  bool escaped = false;
  int depth = 0;
  std::size_t objectStart = std::string::npos;
  for (std::size_t i = arrayStart + 1; i < json.size(); ++i) {
    const char c = json[i];
    if (inString) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') {
      inString = true;
      continue;
    }
    if (c == '{') {
      if (depth == 0) objectStart = i;
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0 && objectStart != std::string::npos) {
        objects.push_back(json.substr(objectStart, i - objectStart + 1));
        objectStart = std::string::npos;
      }
    } else if (c == ']' && depth == 0) {
      break;
    }
  }
  return objects;
}

std::string formatCount(long long value) {
  if (value >= 1000000) {
    std::ostringstream out;
    out.precision(1);
    out << std::fixed << static_cast<double>(value) / 1000000.0 << 'M';
    return out.str();
  }
  if (value >= 1000) {
    std::ostringstream out;
    out.precision(1);
    out << std::fixed << static_cast<double>(value) / 1000.0 << 'k';
    return out.str();
  }
  return std::to_string(value);
}

struct Repository {
  std::string fullName;
  std::string branch;
  std::string url;
  std::string description;
  long long stars = 0;
  long long forks = 0;
  long long watchers = 0;
};

void printRepository(const Repository& repository, std::size_t index) {
  const std::string heading = ascii::boldize(ascii::colorize(repository.fullName, ascii::Color::White));
  Console::print(std::to_string(index) + ". " + heading);
  Console::print("   Default branch: " + ascii::colorize(repository.branch.empty() ? "unknown" : repository.branch, ascii::Color::Gray));
  Console::print("   URL: " + ascii::underline(ascii::colorize(repository.url, ascii::Color::Blue)));
  if (!repository.description.empty()) {
    Console::print("   " + ascii::colorize("About: ", ascii::Color::Green) + repository.description);
  }
  Console::print(
    "   " + ascii::colorize("Stars: ", ascii::Color::Gray) + ascii::colorize(formatCount(repository.stars), ascii::Color::Yellow) +
    "  " + ascii::colorize("Forks: ", ascii::Color::Gray) + ascii::colorize(formatCount(repository.forks), ascii::Color::Yellow) +
    "  " + ascii::colorize("Watchers: ", ascii::Color::Gray) + ascii::colorize(formatCount(repository.watchers), ascii::Color::Yellow)
  );
  Console::print("");
}

}

int SearchCommand::execute(const CLI& cli) {
  if (cli.getArguments().empty()) {
    Console::error("Usage: frankenstein search <query...>");
    return 1;
  }

  std::string query;
  for (const std::string& argument : cli.getArguments()) {
    if (!query.empty()) query += ' ';
    query += argument;
  }

#ifndef FRANKENSTEIN_HAS_CPR
  Console::error("GitHub search is unavailable: this build was compiled without CPR support.");
  return 1;
#else
  const std::string url = "https://api.github.com/search/repositories?q=" + urlEncode(query) +
      "&sort=stars&order=desc&per_page=30";
  const cpr::Response response = cpr::Get(
    cpr::Url{url},
    cpr::Header{{"Accept", "application/vnd.github+json"}, {"User-Agent", "frankenstein"}},
    cpr::Redirect{true},
    cpr::VerifySsl{true},
    cpr::Timeout{30000}
  );
  if (response.error || response.status_code != 200) {
    Console::error("GitHub search failed (HTTP " + std::to_string(response.status_code) + ").");
    return 1;
  }

  std::vector<Repository> repositories;
  for (const std::string& object : jsonObjects(response.text)) {
    Repository repository;
    repository.fullName = jsonString(object, "full_name");
    repository.branch = jsonString(object, "default_branch");
    repository.url = jsonString(object, "html_url");
    repository.description = jsonString(object, "description");
    repository.stars = jsonNumber(object, "stargazers_count");
    repository.forks = jsonNumber(object, "forks_count");
    repository.watchers = jsonNumber(object, "watchers_count");
    if (!repository.fullName.empty() && !repository.url.empty()) repositories.push_back(std::move(repository));
  }

  std::stable_sort(repositories.begin(), repositories.end(), [](const Repository& left, const Repository& right) {
    return left.stars > right.stars;
  });
  if (repositories.empty()) {
    Console::print("No repositories found for: " + query);
    return 0;
  }

  Console::print("GitHub repositories matching: " + query + "\n");
  const std::size_t count = std::min<std::size_t>(5, repositories.size());
  for (std::size_t i = 0; i < count; ++i) printRepository(repositories[i], i + 1);
  return 0;
#endif
}
