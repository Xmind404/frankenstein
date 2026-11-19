#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "cli.h"
#include "../commands/command.h"

class CommandHandler {
  private:
  std::unordered_map<std::string, std::unique_ptr<Command>> commands;

  int dispatchCommand(const CLI& cli);

  public:
  CommandHandler();

  void registerCommand(const std::string& name, std::unique_ptr<Command> command);

  int execute(const CLI& cli);
};