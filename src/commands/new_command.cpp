#include "new_command.h"

#include "project_validator.h"
#include "../cli/console.h"

int NewCommand::execute(const CLI& cli) {
    const std::vector<std::string>& arguments = cli.getArguments();

    if (arguments.empty()) {
        Console::error("Missing project name.");
        Console::print("Usage: frankenstein new <project-name>");

        return 1;
    }

    const std::string& project_name = arguments[0];

    if (!ProjectValidator::isValidName(project_name)) {
        Console::error("Invalid project name: " + project_name);
        Console::print("Project names may contain letters, numbers, hyphens and underscores.");

        return 1;
    }

    Console::success("Project name: " + project_name);

    return 0;
}