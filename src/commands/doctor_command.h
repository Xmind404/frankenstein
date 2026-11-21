#pragma once

#include "command.h"

class DoctorCommand : public Command {
public:
  int execute(const CLI& cli) override;
};
