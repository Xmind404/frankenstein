#pragma once

#include <string>
#include <vector>

namespace ascii {

  enum class Color {
    Default,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Gray,
    DarkGreen,
    BrightGreen,
    DarkBlue,
    BrightBlue,
    Purple,
    Orange,
    Pink
  };

  void draw(const std::string& content);
  void draw(const std::string& content, Color color);

  void draw(
    const std::string& symbol,
    const std::string& content,
    Color symbol_color = Color::Default,
    Color content_color = Color::Default
  );

  void draw(const std::vector<std::string>& lines, Color color = Color::Default);
  std::string colorize(const std::string& content, Color color);
  void drawLogo();
  void setColorEnabled(bool enabled);

}
