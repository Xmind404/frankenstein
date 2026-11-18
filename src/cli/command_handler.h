#pragma once

#include "cli.h"

class CommandHandler {
  private:
    CLI cli;

    int dispatchCommand();

  public:
    explicit CommandHandler(CLI cli);

    int execute();
};