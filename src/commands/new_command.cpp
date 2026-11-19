
#include "new_command.h"

#include "project_validator.h"
#include "project_creator.h"
#include "project_structure.h"
#include "project_template.h"

#include "../cli/console.h"
#include "../cli/prompt.h"
#include "../configuration/project_configuration.h"

#include <filesystem>
#include <string>
#include <vector>

int NewCommand::execute(const CLI& cli) {
    const auto& arguments = cli.getArguments();

    std::string project_name;

    if (arguments.empty()) {
        project_name = prompt::input("Project name:", "my-project");
    } else {
        project_name = arguments[0];
    }

    if (!ProjectValidator::isValidName(project_name)) {
        Console::error("Invalid project name: " + project_name);
        return 1;
    }

    const std::filesystem::path project_path{project_name};

    const std::vector<std::string> languages = {
        "C++",
        "C"
    };

    const std::size_t selected_language = prompt::select(
        "Select a programming language:",
        languages,
        0
    );

    configuration::ProjectConfiguration project_configuration(
        configuration::CppStandard::CXX20
    );

    if (selected_language == 0) {
        const std::vector<std::string> standards = {
            "CXX26",
            "CXX23",
            "CXX20",
            "CXX17",
            "CXX14",
            "CXX11",
            "CXX03",
            "CXX98"
        };

        const std::vector<configuration::CppStandard> cpp_standards = {
            configuration::CppStandard::CXX26,
            configuration::CppStandard::CXX23,
            configuration::CppStandard::CXX20,
            configuration::CppStandard::CXX17,
            configuration::CppStandard::CXX14,
            configuration::CppStandard::CXX11,
            configuration::CppStandard::CXX03,
            configuration::CppStandard::CXX98
        };

        const std::size_t selected_standard = prompt::select(
            "Select a C++ standard:",
            standards,
            2
        );

        project_configuration = configuration::ProjectConfiguration(
            cpp_standards[selected_standard]
        );
    } else {
        const std::vector<std::string> standards = {
            "C23",
            "C17",
            "C11",
            "C99",
            "C89"
        };

        const std::vector<configuration::CStandard> c_standards = {
            configuration::CStandard::C23,
            configuration::CStandard::C17,
            configuration::CStandard::C11,
            configuration::CStandard::C99,
            configuration::CStandard::C89
        };

        const std::size_t selected_standard = prompt::select(
            "Select a C standard:",
            standards,
            1
        );

        project_configuration = configuration::ProjectConfiguration(
            c_standards[selected_standard]
        );
    }

    ProjectCreator project_creator;

    if (!project_creator.create(project_path)) {
        Console::error("Could not create project: " + project_name);
        return 1;
    }

    ProjectStructure project_structure;

    if (!project_structure.create(project_path)) {
        Console::error("Could not create project structure: " + project_name);
        return 1;
    }

    ProjectTemplate project_template;

    if (!project_template.create(project_path, project_configuration)) {
        Console::error("Could not create project files: " + project_name);
        return 1;
    }

    Console::success("Project created successfully: " + project_name);

    return 0;
}
