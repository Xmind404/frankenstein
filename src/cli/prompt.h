#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace prompt {

  std::string input(
    const std::string& label,
    const std::string& default_value = ""
  );

  std::size_t select(
    const std::string& label,
    const std::vector<std::string>& options,
    std::size_t default_index = 0
  );

}