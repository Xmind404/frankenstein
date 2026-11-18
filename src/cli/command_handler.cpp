#include "command_handler.h"

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
        return 1;
    }

    return iterator->second->execute(cli);
}