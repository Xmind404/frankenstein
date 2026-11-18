#include "cli.h"

void CLI::parseArguments(int argc, char* argv[]) {
    command.clear();
    arguments.clear();
    flags.clear();

    if (argc < 2) {
        return;
    }

    command = argv[1];

    for (int i = 2; i < argc; i++) {
        std::string argument = argv[i];

        if (argument.starts_with("--")) {
            flags.push_back(argument);
        } else {
            arguments.push_back(argument);
        }
    }
}

bool CLI::validateCommand() {
    return !command.empty();
}

bool CLI::validateArguments() {
    return true;
}

bool CLI::parse(int argc, char* argv[]) {
    parseArguments(argc, argv);

    is_valid = validateCommand() && validateArguments();

    return is_valid;
}

std::string CLI::getCommand() const {
    return command;
}

const std::vector<std::string>& CLI::getArguments() const {
    return arguments;
}

bool CLI::hasFlag(const std::string& flag) const {
    for (const std::string& current_flag : flags) {
        if (current_flag == flag) {
            return true;
        }
    }

    return false;
}

bool CLI::isValid() const {
    return is_valid;
}