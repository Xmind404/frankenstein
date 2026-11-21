#include "cli.h"

#include "console.h"
#include <algorithm>
#include <cctype>

namespace {

std::string normalizeOption(std::string option) {
  if (!option.starts_with("--")) {
    return option;
  }
  std::transform(option.begin() + 2, option.end(), option.begin() + 2,
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return option;
}

bool isValuedFlag(const std::string& flag) {
  return flag == "--template" ||
         flag == "--jobs" ||
         flag == "--target" ||
         flag == "--color" ||
         flag == "--rev" ||
         flag == "--sha256";
}

bool isKnownFlag(const std::string& flag) {
  return isValuedFlag(flag) ||
         flag == "--release" ||
         flag == "--verbose" ||
         flag == "--offline" ||
         flag == "--locked";
}

}

void CLI::parseArguments(int argc, char* argv[]) {
  command.clear();
  arguments.clear();
  flags.clear();
  flag_values.clear();
  help_requested = false;
  version_requested = false;

  if (argc < 2) {
    help_requested = true;
    return;
  }

  int start = 1;
  while (start < argc) {
    const std::string option = normalizeOption(argv[start]);
    if (option == "--help" || option == "-h") {
      help_requested = true;
      ++start;
      continue;
    }
    if (option == "--version" || option == "-V") {
      version_requested = true;
      ++start;
      continue;
    }
    if (option == "--color") {
      flags.push_back(option);
      if (start + 1 < argc) {
        flag_values[option] = argv[++start];
      }
      ++start;
      continue;
    }
    if (option == "--verbose" || option == "--offline" || option == "--locked") {
      flags.push_back(option);
      ++start;
      continue;
    }
    break;
  }

  if (start == argc) {
    if (!version_requested) {
      help_requested = true;
    }
    return;
  }

  if (std::string(argv[start]) == "help") {
    help_requested = true;
    if (start + 1 < argc) {
      arguments.push_back(argv[start + 1]);
    }
    return;
  }

  command = argv[start++];
  bool passThrough = false;

  for (int i = start; i < argc; ++i) {
    std::string argument = normalizeOption(argv[i]);
    if (passThrough) {
      arguments.push_back(argument);
      continue;
    }
    if (argument == "--") {
      passThrough = true;
      continue;
    }
    if (argument == "--help" || argument == "-h") {
      help_requested = true;
      continue;
    }
    if (argument == "--version" || argument == "-V") {
      version_requested = true;
      continue;
    }

    if (argument.starts_with("--")) {
      flags.push_back(argument);
      if (isValuedFlag(argument) && i + 1 < argc) {
        const std::string value = argv[i + 1];
        if (!value.starts_with("--")) {
          flag_values[argument] = value;
          ++i;
        }
      }
    } else {
      arguments.push_back(argument);
    }
  }
}

bool CLI::validateCommand() {
  return !command.empty();
}

bool CLI::validateArguments() {
  std::vector<std::string> seen;
  for (const std::string& flag : flags) {
    if (!isKnownFlag(flag)) {
      Console::error("Unknown option: " + flag);
      return false;
    }
    if (std::find(seen.begin(), seen.end(), flag) != seen.end()) {
      Console::error("Option may only be used once: " + flag);
      return false;
    }
    seen.push_back(flag);
    if (isValuedFlag(flag) && getFlagValue(flag).empty()) {
      Console::error("Option requires a value: " + flag);
      return false;
    }
  }

  if (hasFlag("--color") &&
      getFlagValue("--color") != "auto" &&
      getFlagValue("--color") != "always" &&
      getFlagValue("--color") != "never") {
    Console::error("--color must be auto, always, or never.");
    return false;
  }

  if (hasFlag("--jobs")) {
    for (char c : getFlagValue("--jobs")) {
      if (c < '0' || c > '9') {
        Console::error("--jobs expects a positive integer.");
        return false;
      }
    }
  }

  return true;
}

bool CLI::parse(int argc, char* argv[]) {
  parseArguments(argc, argv);
  is_valid = help_requested || version_requested || (validateCommand() && validateArguments());
  return is_valid;
}

const std::string& CLI::getCommand() const {
  return command;
}

const std::vector<std::string>& CLI::getArguments() const {
  return arguments;
}

const std::vector<std::string>& CLI::getFlags() const {
  return flags;
}

bool CLI::hasFlag(const std::string& flag) const {
  return std::find(flags.begin(), flags.end(), flag) != flags.end();
}

const std::string& CLI::getFlagValue(const std::string& flag) const {
  static const std::string empty;
  const auto iterator = flag_values.find(flag);
  if (iterator == flag_values.end()) {
    return empty;
  }
  return iterator->second;
}

bool CLI::isValid() const {
  return is_valid;
}

bool CLI::wantsHelp() const {
  return help_requested;
}

bool CLI::wantsVersion() const {
  return version_requested;
}