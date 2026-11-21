#pragma once

#include "command.h"

class SearchCommand : public Command {
public:
  int execute(const CLI& cli) override;
};
