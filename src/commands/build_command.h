#pragma once
#include "command.h"

class BuildCommand : public Command {
  public:
    enum class Action { Build, Clean, Check, Test };
    explicit BuildCommand(Action action = Action::Build) : action(action) {}
    int execute(const CLI& cli) override;
  private:
    Action action;
};
