#pragma once

#include <string>
#include <vector>

class CLI {
  private:
    std::string command;
    std::vector<std::string> arguments;
    std::vector<std::string> flags;
    bool is_valid = false;

    void parseArguments(int argc, char* argv[]);

    bool validateCommand();
    bool validateArguments();

  public:
    bool parse(int argc, char* argv[]);

    std::string getCommand() const;
    const std::vector<std::string>& getArguments() const;

    bool hasFlag(const std::string& flag) const;
    bool isValid() const;
};