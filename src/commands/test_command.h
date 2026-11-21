#pragma once

#include "command.h"

class TestCommand : public Command {
public:
  int execute(const CLI& cli) override;
};