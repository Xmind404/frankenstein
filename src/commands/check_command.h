#pragma once

#include "command.h"

class CheckCommand : public Command {
public:
  int execute(const CLI& cli) override;
};