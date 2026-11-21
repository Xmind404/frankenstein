#pragma once

#include "command.h"

class PublishCommand : public Command {
public:
  int execute(const CLI& cli) override;
};