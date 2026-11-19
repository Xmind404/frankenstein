#pragma once

#include <optional>
#include <string>

namespace configuration {

    enum class Language {
        C,
        Cpp
    };

    enum class CStandard {
        C89,
        C99,
        C11,
        C17,
        C23
    };

    enum class CppStandard {
        CXX98,
        CXX03,
        CXX11,
        CXX14,
        CXX17,
        CXX20,
        CXX23,
        CXX26
    };

    std::string toString(CStandard standard);
    std::string toString(CppStandard standard);

    class ProjectConfiguration {
      private:
        Language language;
        CStandard c_standard = CStandard::C17;
        CppStandard cpp_standard = CppStandard::CXX20;

      public:
        explicit ProjectConfiguration(CStandard standard);
        explicit ProjectConfiguration(CppStandard standard);

        static std::optional<ProjectConfiguration> fromTemplate(
            const std::string& template_name
        );

        Language getLanguage() const;

        CStandard getCStandard() const;
        CppStandard getCppStandard() const;
    };

}