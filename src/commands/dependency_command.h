#pragma once
#include "command.h"

class DependencyCommand : public Command {
  public:
  enum class Action { Add, Remove, Install, Update, List };
  explicit DependencyCommand(Action action) : action(action) {}
  int execute(const CLI& cli) override;
  private:
  Action action;
};
