cmake_minimum_required(VERSION 3.24)
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
  src/commands/project_initializer.cpp
  src/commands/init_command.cpp
  src/configuration/project_configuration_prompt.cpp
  src/commands/build_command.cpp
  src/commands/run_command.cpp
  src/commands/dependency_command.cpp
  src/commands/diagnostics_command.cpp
)

target_include_directories(frankenstein PRIVATE src)

# CPR is vendored with the repository and is the HTTPS transport for GitHub ZIPs.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cpr/CMakeLists.txt")
  set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CPR_BUILD_TESTS_SSL OFF CACHE BOOL "" FORCE)
  set(CPR_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  # Build CPR with its bundled curl instead of requiring system libcurl headers.
  set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
  set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
  set(CPR_CURL_USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
  add_subdirectory(external/cpr EXCLUDE_FROM_ALL)
  target_compile_definitions(frankenstein PRIVATE FRANKENSTEIN_HAS_CPR=1)
  target_link_libraries(frankenstein PRIVATE cpr::cpr)
endif()
