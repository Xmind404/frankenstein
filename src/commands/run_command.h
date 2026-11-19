#pragma once
#include "command.h"
class RunCommand : public Command { public: int execute(const CLI& cli) override; };
