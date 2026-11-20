#pragma once

#include <string>

#include "command.h"

class SourceCommand : public Command {
public:
  enum class Action { Add, Remove };

  explicit SourceCommand(Action action) : action(action) {}

  int execute(const CLI& cli) override;

private:
  int addSource(const std::string& argument, const CLI& cli);
  int removeSource(const std::string& argument);

  Action action;
};