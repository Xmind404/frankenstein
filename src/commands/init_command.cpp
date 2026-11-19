#include "init_command.h"

#include "project_initializer.h"

#include "../cli/console.h"
#include "../configuration/project_configuration_prompt.h"

#include <filesystem>
#include <optional>
#include <string>

int InitCommand::execute(const CLI& cli) {
  std::filesystem::path project_path = std::filesystem::current_path();
  if (!cli.getArguments().empty()) {
    project_path = cli.getArguments()[0];
    if (project_path.is_relative()) project_path = std::filesystem::current_path() / project_path;
  }

  std::error_code path_error;
  if (!std::filesystem::exists(project_path, path_error) || path_error || !std::filesystem::is_directory(project_path, path_error) || path_error) {
    Console::error("init expects an existing directory.");
    return 1;
  }
  project_path = std::filesystem::weakly_canonical(project_path, path_error);
  if (path_error) { Console::error("Could not resolve project directory: " + path_error.message()); return 1; }

  const std::string project_name =
    project_path.filename().string();

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

  if (!project_initializer.initialize(
    project_path,
    *project_configuration,
    ProjectInitializer::Mode::CurrentDirectory
  )) {
    Console::error("Could not initialize project: " + project_name);
    return 1;
  }

  Console::success("Project initialized successfully: " + project_name);

  return 0;
}
