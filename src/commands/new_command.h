#pragma once

#include "command.h"

class NewCommand : public Command {
  public:
    int execute(const CLI& cli) override;
};