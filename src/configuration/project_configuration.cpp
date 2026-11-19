
#include "project_configuration.h"

namespace configuration {

    std::string toString(CStandard standard) {
        switch (standard) {
            case CStandard::C89:
                return "90";

            case CStandard::C99:
                return "99";

            case CStandard::C11:
                return "11";

            case CStandard::C17:
                return "17";

            case CStandard::C23:
                return "23";
        }

        return "";
    }

    std::string toString(CppStandard standard) {
        switch (standard) {
            case CppStandard::CXX98:
                return "98";

            case CppStandard::CXX03:
                return "98";

            case CppStandard::CXX11:
                return "11";

            case CppStandard::CXX14:
                return "14";

            case CppStandard::CXX17:
                return "17";

            case CppStandard::CXX20:
                return "20";

            case CppStandard::CXX23:
                return "23";

            case CppStandard::CXX26:
                return "26";
        }

        return "";
    }

    ProjectConfiguration::ProjectConfiguration(CStandard standard)
        : language(Language::C), c_standard(standard) {
    }

    ProjectConfiguration::ProjectConfiguration(CppStandard standard)
        : language(Language::Cpp), cpp_standard(standard) {
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
