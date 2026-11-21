#pragma once

#include "command.h"

class CleanCommand : public Command {
public:
  int execute(const CLI& cli) override;
};