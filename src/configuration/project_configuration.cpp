#include "project_configuration.h"

#include <optional>

namespace configuration {

    std::string toString(CStandard standard) {
        switch (standard) {
            case CStandard::C89: return "90";
            case CStandard::C99: return "99";
            case CStandard::C11: return "11";
            case CStandard::C17: return "17";
            case CStandard::C23: return "23";
        }

        return "";
    }

    std::string toString(CppStandard standard) {
        switch (standard) {
            case CppStandard::CXX98: return "98";
            case CppStandard::CXX03: return "98";
            case CppStandard::CXX11: return "11";
            case CppStandard::CXX14: return "14";
            case CppStandard::CXX17: return "17";
            case CppStandard::CXX20: return "20";
            case CppStandard::CXX23: return "23";
            case CppStandard::CXX26: return "26";
        }

        return "";
    }

    ProjectConfiguration::ProjectConfiguration(CStandard standard)
        : language(Language::C), c_standard(standard) {
    }

    ProjectConfiguration::ProjectConfiguration(CppStandard standard)
        : language(Language::Cpp), cpp_standard(standard) {
    }

    std::optional<ProjectConfiguration> ProjectConfiguration::fromTemplate(
        const std::string& template_name
    ) {
        if (template_name == "C89") {
            return ProjectConfiguration(CStandard::C89);
        }

        if (template_name == "C99") {
            return ProjectConfiguration(CStandard::C99);
        }

        if (template_name == "C11") {
            return ProjectConfiguration(CStandard::C11);
        }

        if (template_name == "C17") {
            return ProjectConfiguration(CStandard::C17);
        }

        if (template_name == "C23") {
            return ProjectConfiguration(CStandard::C23);
        }

        if (template_name == "CXX98") {
            return ProjectConfiguration(CppStandard::CXX98);
        }

        if (template_name == "CXX03") {
            return ProjectConfiguration(CppStandard::CXX03);
        }

        if (template_name == "CXX11") {
            return ProjectConfiguration(CppStandard::CXX11);
        }

        if (template_name == "CXX14") {
            return ProjectConfiguration(CppStandard::CXX14);
        }

        if (template_name == "CXX17") {
            return ProjectConfiguration(CppStandard::CXX17);
        }

        if (template_name == "CXX20") {
            return ProjectConfiguration(CppStandard::CXX20);
        }

        if (template_name == "CXX23") {
            return ProjectConfiguration(CppStandard::CXX23);
        }

        if (template_name == "CXX26") {
            return ProjectConfiguration(CppStandard::CXX26);
        }

        return std::nullopt;
    }

    Language ProjectConfiguration::getLanguage() const {
        return language;
    }

    CStandard ProjectConfiguration::getCStandard() const {
        return c_standard;
    }

    CppStandard ProjectConfiguration::getCppStandard() const {
        return cpp_standard;
    }

}