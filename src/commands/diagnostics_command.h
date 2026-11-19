#pragma once
#include "command.h"

class DiagnosticsCommand : public Command {
  public:
  enum class Action { Doctor, Info, Search };
  explicit DiagnosticsCommand(Action action) : action(action) {}
  int execute(const CLI& cli) override;
  private:
  Action action;
};
