#include "cli.h"

#include "console.h"
#include <algorithm>
#include <cctype>

namespace {
std::string normalizeOption(std::string option) {
  if (!option.starts_with("--")) return option;
  std::transform(option.begin() + 2, option.end(), option.begin() + 2,
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return option;
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
    if (option == "--help" || option == "-h") { help_requested = true; ++start; continue; }
    if (option == "--version" || option == "-V") { version_requested = true; ++start; continue; }
    if (option == "--color") {
      flags.push_back(option);
      if (start + 1 < argc) flag_values[option] = argv[++start];
      ++start;
      continue;
    }
    if (option == "--verbose" || option == "--offline" || option == "--locked") { flags.push_back(option); ++start; continue; }
    break;
  }
  if (start == argc) {
    if (!version_requested) help_requested = true;
    return;
  }
  if (std::string(argv[start]) == "help") {
    help_requested = true;
    if (start + 1 < argc) arguments.push_back(argv[start + 1]);
    return;
  }
  command = argv[start++];
  bool pass_through = false;

  for (int i = start; i < argc; i++) {
    std::string argument = normalizeOption(argv[i]);
    if (pass_through) { arguments.push_back(argument); continue; }
    if (argument == "--") { pass_through = true; continue; }

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

      if ((argument == "--template" || argument == "--branch" || argument == "--jobs" || argument == "--target" || argument == "--color" || argument == "--std" || argument == "--rev" || argument == "--sha256") && i + 1 < argc) {
        std::string value = argv[i + 1];

        if (!value.starts_with("--")) {
          flag_values[argument] = value;
          i++;
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
  const std::vector<std::string> valued = {"--template", "--branch", "--jobs", "--target", "--color", "--std", "--rev", "--sha256"};
  const std::vector<std::string> known = {
    "--template", "--branch", "--jobs", "--target", "--color", "--std", "--rev", "--sha256",
    "--release", "--debug", "--verbose", "--offline", "--locked"
  };
  std::vector<std::string> seen;
  for (const std::string& flag : flags) {
    if (std::find(known.begin(), known.end(), flag) == known.end()) {
      Console::error("Unknown option: " + flag);
      return false;
    }
    if (std::find(seen.begin(), seen.end(), flag) != seen.end()) {
      Console::error("Option may only be used once: " + flag);
      return false;
    }
    seen.push_back(flag);
    if (std::find(valued.begin(), valued.end(), flag) != valued.end() && getFlagValue(flag).empty()) {
      Console::error("Option requires a value: " + flag);
      return false;
    }
  }
  if (hasFlag("--release") && hasFlag("--debug")) { Console::error("Choose either --release or --debug."); return false; }
  if (hasFlag("--color") && getFlagValue("--color") != "auto" && getFlagValue("--color") != "always" && getFlagValue("--color") != "never") {
    Console::error("--color must be auto, always, or never."); return false;
  }
  if (hasFlag("--jobs")) for (char c : getFlagValue("--jobs")) if (c < '0' || c > '9') { Console::error("--jobs expects a positive integer."); return false; }

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
  for (const std::string& current_flag : flags) {
    if (current_flag == flag) {
      return true;
    }
  }

  return false;
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

bool CLI::wantsHelp() const { return help_requested; }
bool CLI::wantsVersion() const { return version_requested; }
