#pragma once

#include "../cli/cli.h"

class Command {
  public:
  virtual ~Command() = default;

  virtual int execute(const CLI& cli) = 0;
};