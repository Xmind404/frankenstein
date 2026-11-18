#include "command_handler.h"

CommandHandler::CommandHandler(CLI cli) 
    : cli(cli){
}

int CommandHandler::execute() {
  if (!cli.isValid()) {
    return 1;
  }

  return dispatchCommand();
}


int CommandHandler::dispatchCommand() {
  const std::string command = cli.getCommand();

  if (command.empty()) {
    return 1;
  }

  return 0;
}