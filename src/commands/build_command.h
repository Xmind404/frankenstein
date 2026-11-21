#pragma once

#include "command.h"

class BuildCommand : public Command {
public:
  int execute(const CLI& cli) override;

  static std::string configurationFromCLI(const CLI& cli);
  static std::string quoteShellArgument(const std::string& value);
  static int runCMake(const std::string& arguments);
};