cmake_minimum_required(VERSION 3.16)
project(frankenstein CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(frankenstein 
  src/main.cpp
  src/datatypes/version.cpp
  src/core/application.cpp
  src/commands/new_command.cpp
  src/cli/cli.cpp
  src/cli/ascii.cpp
  src/cli/command_handler.cpp
  src/cli/console.cpp
  src/commands/project_validator.cpp
  src/commands/project_creator.cpp
  src/commands/project_structure.cpp
  src/commands/project_template.cpp
  src/cli/prompt.cpp
  src/configuration/project_configuration.cpp
)

target_include_directories(frankenstein PRIVATE src)