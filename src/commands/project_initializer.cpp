#include "project_initializer.h"

#include "project_creator.h"
#include "project_structure.h"
#include "project_template.h"

bool ProjectInitializer::initialize(
  const std::filesystem::path& project_path,
  const configuration::ProjectConfiguration& configuration,
  Mode mode
) {
  if (mode == Mode::NewProject) {
    ProjectCreator project_creator;

    if (!project_creator.create(project_path)) {
      return false;
    }
  }

  ProjectStructure project_structure;

  if (!project_structure.create(project_path)) {
    return false;
  }

  ProjectTemplate project_template;

  if (!project_template.create(project_path, configuration)) {
    return false;
  }

  return true;
}