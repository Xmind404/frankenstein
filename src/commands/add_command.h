#pragma once

#include "command.h"

class AddCommand : public Command {
public:
  int execute(const CLI& cli) override;

private:
  int addDependency(const CLI& cli, const std::string& argument);
  int addSource(const CLI& cli, const std::string& argument);
};
