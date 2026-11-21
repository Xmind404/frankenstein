#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class CLI {
private:
  std::string command;
  std::vector<std::string> arguments;
  std::vector<std::string> flags;
  std::unordered_map<std::string, std::string> flag_values;
  bool is_valid = false;
  bool help_requested = false;
  bool version_requested = false;

  void parseArguments(int argc, char* argv[]);
  bool validateCommand();
  bool validateArguments();

public:
  bool parse(int argc, char* argv[]);

  const std::string& getCommand() const;
  const std::vector<std::string>& getArguments() const;
  const std::vector<std::string>& getFlags() const;

  bool hasFlag(const std::string& flag) const;
  const std::string& getFlagValue(const std::string& flag) const;

  bool isValid() const;
  bool wantsHelp() const;
  bool wantsVersion() const;
};