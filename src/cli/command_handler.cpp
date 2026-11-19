#include "command_handler.h"
#include "console.h"

CommandHandler::CommandHandler() = default;

void CommandHandler::registerCommand(const std::string& name, std::unique_ptr<Command> command) {
  commands.insert_or_assign(name, std::move(command));
}

int CommandHandler::execute(const CLI& cli) {
  if (!cli.isValid()) {
    return 1;
  }

  return dispatchCommand(cli);
}

int CommandHandler::dispatchCommand(const CLI& cli) {
  const auto iterator = commands.find(cli.getCommand());

  if (iterator == commands.end()) {
    Console::error("Unknown command: " + cli.getCommand() + ". Run 'frankenstein --help' for a list of commands.");
    return 127;
  }

  return iterator->second->execute(cli);
}
