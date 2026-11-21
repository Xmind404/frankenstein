#pragma once

#include "command.h"

class InitCommand : public Command {
public:
  int execute(const CLI& cli) override;
};