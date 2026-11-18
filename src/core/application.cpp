#include "application.h"

#include <memory>

#include "../cli/cli.h"
#include "../cli/command_handler.h"
#include "../commands/new_command.h"

Application::Application(datatypes::Version version)
    : version(version) {
}

const datatypes::Version& Application::getVersion() const {
    return version;
}

int Application::run(int argc, char* argv[]) {
    CLI cli;

    if (!cli.parse(argc, argv)) {
        return 1;
    }

    CommandHandler commandHandler;

    commandHandler.registerCommand("new", std::make_unique<NewCommand>());

    return commandHandler.execute(cli);
}