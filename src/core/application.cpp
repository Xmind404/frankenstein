#include "application.h"

#include "../cli/cli.h"
#include "../cli/command_handler.h"

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

    CommandHandler command_handler(cli);

    return command_handler.execute();
}