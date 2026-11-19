#pragma once

#include <filesystem>
#include <string>

namespace configuration {

  bool synchronizeProjectStandard(
    const std::filesystem::path& project_root,
    std::string& message
  );

}
