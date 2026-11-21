#pragma once

#include "command.h"

class InfoCommand : public Command {
public:
  int execute(const CLI& cli) override;
};