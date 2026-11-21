#pragma once

#include "command.h"

class UpdateCommand : public Command {
public:
  int execute(const CLI& cli) override;
};