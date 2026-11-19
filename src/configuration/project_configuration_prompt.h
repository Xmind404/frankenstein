#pragma once

#include "project_configuration.h"

namespace configuration {

    class ProjectConfigurationPrompt {
      public:
        static ProjectConfiguration select();
    };

}