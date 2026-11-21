#pragma once

#include "command.h"

class RemoveCommand : public Command {
public:
  int execute(const CLI& cli) override;

private:
  int removeDependency(const CLI& cli, const std::string& argument);
  int removeSource(const CLI& cli, const std::string& argument);
};
