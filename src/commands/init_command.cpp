#include "init_command.h"

#include "project_initializer.h"

#include "../cli/console.h"
#include "../configuration/project_configuration_prompt.h"

#include <filesystem>
#include <optional>
#include <string>

int InitCommand::execute(const CLI& cli) {
  if (!cli.getArguments().empty()) {
    Console::error("frankenstein init does not accept arguments. Run it inside the directory you want to initialize.");
    return 1;
  }

  const std::filesystem::path project_path = std::filesystem::current_path();
  const std::string project_name = project_path.filename().string();

  std::optional<configuration::ProjectConfiguration> project_configuration;
  if (cli.hasFlag("--template")) {
    project_configuration = configuration::ProjectConfiguration::fromTemplate(
      cli.getFlagValue("--template")
    );
    if (!project_configuration.has_value()) {
      Console::error("Unknown template: " + cli.getFlagValue("--template"));
      return 1;
    }
  } else {
    project_configuration = configuration::ProjectConfigurationPrompt::select();
  }

  ProjectInitializer project_initializer;
  if (!project_initializer.initialize(
    project_path,
    *project_configuration,
    ProjectInitializer::Mode::CurrentDirectory
  )) {
    Console::error("Could not initialize project: " + project_name);
    return 1;
  }

  Console::success("Project initialized successfully: " + project_name);
  Console::print("Next: frankenstein sync && frankenstein build");
  return 0;
}