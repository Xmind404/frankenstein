#include "new_command.h"

#include "project_initializer.h"
#include "project_validator.h"

#include "../cli/console.h"
#include "../cli/prompt.h"
#include "../configuration/project_configuration_prompt.h"

#include <filesystem>
#include <optional>
#include <string>

int NewCommand::execute(const CLI& cli) {
    const auto& arguments = cli.getArguments();

    const bool initialize_current_directory =
        !arguments.empty() && arguments[0] == ".";

    std::string project_name;

    if (initialize_current_directory) {
        project_name = std::filesystem::current_path().filename().string();
    } else if (arguments.empty()) {
        project_name = prompt::input("Project name:", "my-project");
    } else {
        project_name = arguments[0];
    }

    if (!ProjectValidator::isValidName(project_name)) {
        Console::error("Invalid project name: " + project_name);
        return 1;
    }

    const std::filesystem::path project_path =
        initialize_current_directory
            ? std::filesystem::current_path()
            : std::filesystem::path{project_name};

    std::optional<configuration::ProjectConfiguration> project_configuration;

    if (cli.hasFlag("--template")) {
        project_configuration = configuration::ProjectConfiguration::fromTemplate(
            cli.getFlagValue("--template")
        );

        if (!project_configuration.has_value()) {
            Console::error(
                "Unknown template: " + cli.getFlagValue("--template")
            );

            return 1;
        }
    } else {
        project_configuration =
            configuration::ProjectConfigurationPrompt::select();
    }

    ProjectInitializer project_initializer;

    const auto mode = initialize_current_directory
        ? ProjectInitializer::Mode::CurrentDirectory
        : ProjectInitializer::Mode::NewProject;

    if (!project_initializer.initialize(
        project_path,
        *project_configuration,
        mode
    )) {
        Console::error("Could not initialize project: " + project_name);
        return 1;
    }

    Console::success("Project created successfully: " + project_name);

    return 0;
}