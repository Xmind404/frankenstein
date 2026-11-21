#include "new_command.h"

#include "project_initializer.h"
#include "project_validator.h"

#include "../cli/console.h"
#include "../cli/prompt.h"
#include "../configuration/project_configuration_prompt.h"

#include <filesystem>
#include <optional>
#include <string>

namespace {

std::string availableDefaultProjectName() {
  namespace fs = std::filesystem;
  for (std::size_t suffix = 1;; ++suffix) {
    const std::string name = suffix == 1 ? "my-project" : "my-project-" + std::to_string(suffix);
    std::error_code error;
    const bool exists = fs::exists(name, error);
    if (error || !exists) {
      return name;
    }
  }
}

}

int NewCommand::execute(const CLI& cli) {
  const auto& arguments = cli.getArguments();

  std::string project_name;
  if (arguments.empty()) {
    project_name = prompt::input("Project name:", availableDefaultProjectName());
  } else {
    project_name = arguments[0];
  }

  if (!ProjectValidator::isValidName(project_name)) {
    Console::error("Invalid project name: " + project_name);
    return 1;
  }

  const std::filesystem::path project_path{project_name};

  std::error_code error;
  if (std::filesystem::exists(project_path, error)) {
    Console::error(
      "Project already exists: " + project_name +
      ". Choose another name or use 'frankenstein init' inside that directory."
    );
    return 1;
  }
  if (error) {
    Console::error("Could not check project path: " + error.message());
    return 1;
  }

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
    ProjectInitializer::Mode::NewProject
  )) {
    Console::error("Could not initialize project: " + project_name);
    return 1;
  }

  Console::success("Project created successfully: " + project_name);
  Console::print("Next: cd " + project_name + " && frankenstein sync && frankenstein build");
  return 0;
}