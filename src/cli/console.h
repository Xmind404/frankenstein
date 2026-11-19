#pragma once

#include <string>

class Console {
  public:
  static void setColorEnabled(bool enabled);
  static void print(const std::string& message);
  static void success(const std::string& message);
  static void warning(const std::string& message);
  static void error(const std::string& message);
};
