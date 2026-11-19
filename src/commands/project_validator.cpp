#include "project_validator.h"

#include <algorithm>
#include <cctype>

bool ProjectValidator::isValidName(const std::string& name) {
  if (name.empty() || name == "." || name == "..") {
    return false;
  }

  if (name.front() == '-' || name.front() == '_') {
    return false;
  }

  return std::all_of(name.begin(), name.end(), [](unsigned char character) {
    return std::isalnum(character) || character == '-' || character == '_';
  });
}