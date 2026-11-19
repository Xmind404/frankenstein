#include "project_configuration_prompt.h"

#include "../cli/prompt.h"

#include <string>
#include <vector>

namespace configuration {

    ProjectConfiguration ProjectConfigurationPrompt::select() {
        const std::vector<std::string> languages = {
            "C++",
            "C"
        };

        const std::size_t selected_language = prompt::select(
            "Select a programming language:",
            languages,
            0
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

            const std::vector<CppStandard> cpp_standards = {
                CppStandard::CXX26,
                CppStandard::CXX23,
                CppStandard::CXX20,
                CppStandard::CXX17,
                CppStandard::CXX14,
                CppStandard::CXX11,
                CppStandard::CXX03,
                CppStandard::CXX98
            };

            const std::size_t selected_standard = prompt::select(
                "Select a C++ standard:",
                standards,
                2
            );

            return ProjectConfiguration(cpp_standards[selected_standard]);
        }

        const std::vector<std::string> standards = {
            "C23",
            "C17",
            "C11",
            "C99",
            "C89"
        };

        const std::vector<CStandard> c_standards = {
            CStandard::C23,
            CStandard::C17,
            CStandard::C11,
            CStandard::C99,
            CStandard::C89
        };

        const std::size_t selected_standard = prompt::select(
            "Select a C standard:",
            standards,
            1
        );

        return ProjectConfiguration(c_standards[selected_standard]);
    }

}