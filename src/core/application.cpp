#include "application.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "../cli/ascii.h"
#include "../cli/cli.h"
#include "../cli/command_handler.h"
#include "../cli/console.h"
#include "../commands/add_command.h"
#include "../commands/build_command.h"
#include "../commands/check_command.h"
#include "../commands/clean_command.h"
#include "../commands/doctor_command.h"
#include "../commands/info_command.h"
#include "../commands/init_command.h"
#include "../commands/new_command.h"
#include "../commands/publish_command.h"
#include "../commands/remove_command.h"
#include "../commands/run_command.h"
#include "../commands/search_command.h"
#include "../commands/sync_command.h"
#include "../commands/test_command.h"
#include "../commands/tree_command.h"
#include "../commands/update_command.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

bool terminalSupportsColor() {
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(fileno(stdout)) != 0;
#endif
}

void printCommandList() {
  std::cout
    << "Project creation:\n"
    << "  new         Create a project in a new directory\n"
    << "  init        Initialize the current directory\n\n"
    << "Dependencies and sources:\n"
    << "  add         Add a GitHub dependency or track a source file\n"
    << "  remove      Remove a dependency or untrack a source file (alias: rm)\n"
    << "  update      Update a dependency to the latest commit on its branch\n"
    << "  sync        Reconcile external/, CMakeLists.txt and Frankenstein.lock\n"
    << "  tree        Display the dependency tree\n\n"
    << "Build and execution:\n"
    << "  build       Configure and compile the project\n"
    << "  check       Configure and verify that the project builds\n"
    << "  test        Build the project and run CTest\n"
    << "  run         Build when needed and run the executable\n"
    << "  publish     Build Release and export the executable to publish/\n"
    << "  clean       Remove build/ without touching dependencies\n\n"
    << "Diagnostics and discovery:\n"
    << "  doctor      Check the local build toolchain\n"
    << "  info        Print Frankenstein.toml\n"
    << "  search      Search GitHub repositories\n\n";
}

}

Application::Application(datatypes::Version version, std::string version_date)
  : version(version), version_date(std::move(version_date)) {
}

const datatypes::Version& Application::getVersion() const {
  return version;
}

int Application::run(int argc, char* argv[]) {
  CLI cli;
  if (!cli.parse(argc, argv)) return 1;

  const std::string colorMode = cli.hasFlag("--color") ? cli.getFlagValue("--color") : "auto";
  const bool colors = colorMode == "always" || (colorMode == "auto" && terminalSupportsColor());
  Console::setColorEnabled(colors);
  ascii::setColorEnabled(colors);

  if (cli.wantsVersion()) {
    std::cout << "frankenstein " << version << " (" << version_date << ")\n";
    return 0;
  }

  if (cli.getCommand() == "help") {
    const std::string target = cli.getArguments().empty() ? std::string{} : cli.getArguments()[0];
    printHelp(target);
    return 0;
  }

  if (cli.wantsHelp() || cli.getCommand().empty()) {
    const std::string target = !cli.getCommand().empty()
      ? cli.getCommand()
      : (cli.getArguments().empty() ? std::string{} : cli.getArguments()[0]);
    printHelp(target);
    return 0;
  }

  CommandHandler handler;
  handler.registerCommand("new", std::make_unique<NewCommand>());
  handler.registerCommand("init", std::make_unique<InitCommand>());
  handler.registerCommand("build", std::make_unique<BuildCommand>());
  handler.registerCommand("publish", std::make_unique<PublishCommand>());
  handler.registerCommand("clean", std::make_unique<CleanCommand>());
  handler.registerCommand("test", std::make_unique<TestCommand>());
  handler.registerCommand("run", std::make_unique<RunCommand>());
  handler.registerCommand("add", std::make_unique<AddCommand>());
  handler.registerCommand("remove", std::make_unique<RemoveCommand>());
  handler.registerCommand("rm", std::make_unique<RemoveCommand>());
  handler.registerCommand("update", std::make_unique<UpdateCommand>());
  handler.registerCommand("check", std::make_unique<CheckCommand>());
  handler.registerCommand("sync", std::make_unique<SyncCommand>());
  handler.registerCommand("tree", std::make_unique<TreeCommand>());
  handler.registerCommand("doctor", std::make_unique<DoctorCommand>());
  handler.registerCommand("info", std::make_unique<InfoCommand>());
  handler.registerCommand("search", std::make_unique<SearchCommand>());

  const int result = handler.execute(cli);
  if (result == 127) printHelp(cli.getCommand());
  return result;
}

void Application::printHelp(const std::string& command) const {
  if (command.empty() || command == "help") {
    ascii::drawLogo();
    std::cout
      << "Frankenstein - C/C++ project and package manager\n\n"
      << "Usage: frankenstein [GLOBAL OPTIONS] <COMMAND> [ARGS...]\n\n"
      << "Global options:\n"
      << "  -h, --help              Show general or command-specific help\n"
      << "  -V, --version           Print version information\n"
      << "      --color <WHEN>      Color output: auto, always or never\n\n";
    printCommandList();
    std::cout
      << "Project files:\n"
      << "  Frankenstein.toml      Project configuration and dependency declarations\n"
      << "  Frankenstein.lock      Resolved dependency commits and archive SHA-256 hashes\n"
      << "  CMakeLists.txt         Generated CMake source/dependency integration blocks\n"
      << "  external/              Installed dependency source trees\n"
      << "  compile_commands.json  Compilation database copied to the project root\n\n"
      << "Typical workflow:\n"
      << "  frankenstein new my-app --template CXX20\n"
      << "  cd my-app\n"
      << "  frankenstein add webview/webview:master --target webview_core_static\n"
      << "  frankenstein sync\n"
      << "  frankenstein build\n\n"
      << "Run 'frankenstein help <command>' for command details.\n";
    return;
  }

  std::cout << "Usage: frankenstein " << command << " [OPTIONS] [ARGS]\n\n";

  if (command == "new") {
    std::cout
      << "Create a new C or C++ project in a new directory.\n\n"
      << "Creates the project directory, the CMake build definition, the Frankenstein\n"
      << "manifest, formatting/editor configuration and the standard source folders.\n"
      << "The generated .clangd file points clangd at the root compilation database:\n\n"
      << "  CompileFlags:\n"
      << "    CompilationDatabase: .\n\n"
      << "Usage:\n  frankenstein new [name] [--template <name>]\n\n"
      << "Templates:\n"
      << "  C89, C99, C11, C17, C23\n"
      << "  CXX98, CXX03, CXX11, CXX14, CXX17, CXX20, CXX23, CXX26\n\n"
      << "Examples:\n"
      << "  frankenstein new my-app --template CXX20\n"
      << "  frankenstein new c-tool --template C17\n";
    return;
  }

  if (command == "init") {
    std::cout
      << "Initialize the current directory as a Frankenstein project.\n\n"
      << "Adds missing project files and directories without overwriting existing files.\n"
      << "The generated .clangd file uses CompilationDatabase: . so clangd reads\n"
      << "compile_commands.json from the project root.\n\n"
      << "Usage:\n  frankenstein init [--template <name>]\n\n"
      << "Example:\n  frankenstein init --template CXX20\n";
    return;
  }

  if (command == "add") {
    std::cout
      << "Add a GitHub dependency or register a source file in Frankenstein.toml.\n\n"
      << "Dependency syntax:\n  frankenstein add <owner/repository[:branch]> [--target <cmake-target>]\n\n"
      << "Downloads the selected branch into external/<repository>/, checks the archive\n"
      << "SHA-256 and detects CMake library targets. If --target is omitted, you can\n"
      << "select a detected target. The dependency is recorded in [dependencies.<name>].\n"
      << "The command never adds a duplicate entry or overwrites an existing directory.\n\n"
      << "Source syntax:\n  frankenstein add <src/file.cpp>\n"
      << "The source must already exist inside the project. It is registered in\n"
      << "[sources].files; the file itself is not copied.\n\n"
      << "Options:\n"
      << "  --target <name>     CMake target to link\n"
      << "  --rev <sha>         Pin a commit (40 or 64 hexadecimal characters)\n"
      << "  --sha256 <hash>     Require the downloaded archive SHA-256\n\n"
      << "Examples:\n"
      << "  frankenstein add webview/webview:master --target webview_core_static\n"
      << "  frankenstein add libcpr/cpr:master\n"
      << "  frankenstein add src/network.cpp\n\n"
      << "Run 'frankenstein sync' after adding a dependency to regenerate CMake and the lockfile.\n";
    return;
  }

  if (command == "remove" || command == "rm") {
    std::cout
      << "Remove a declared dependency or untrack a source file.\n\n"
      << "Dependency syntax:\n  frankenstein remove <owner/repository>\n"
      << "Removes the matching manifest entry, external/<repository>/, and its lockfile\n"
      << "record. If the package directory is already missing, the manifest entry is\n"
      << "still removed. An undeclared dependency is never used as a reason to delete\n"
      << "an arbitrary directory.\n\n"
      << "Source syntax:\n  frankenstein remove <src/file.cpp>\n"
      << "Removes the source from [sources].files but leaves the source file on disk.\n\n"
      << "Alias: rm\n\n"
      << "Examples:\n"
      << "  frankenstein remove webview/webview\n"
      << "  frankenstein rm src/network.cpp\n";
    return;
  }

  if (command == "sync") {
    std::cout
      << "Reconcile the project with Frankenstein.toml. This is the package installation\n"
      << "and CMake synchronization command. Run it after editing the manifest or when\n"
      << "setting up a project on another machine.\n\n"
      << "Dependency reconciliation:\n"
      << "  1. Validate project and dependency declarations before changing files.\n"
      << "  2. For every [dependencies.<name>] entry, install the requested repository\n"
      << "     and branch into external/<name>/. Existing valid lock entries pin exact\n"
      << "     commits; missing or changed lock entries resolve the configured branch.\n"
      << "  3. Remove package directories in external/ that are not declared in the TOML.\n"
      << "     external/ is managed by Frankenstein; do not store unrelated files there.\n"
      << "  4. Rebuild Frankenstein.lock from exactly the dependencies in the manifest.\n\n"
      << "Project/CMake synchronization:\n"
      << "  5. Update the matching C or C++ standard without changing project language.\n"
      << "  6. Regenerate the managed source and dependency blocks in CMakeLists.txt.\n"
      << "  7. Configure build/Debug with CMAKE_EXPORT_COMPILE_COMMANDS=ON and copy\n"
      << "     build/Debug/compile_commands.json to ./compile_commands.json.\n\n"
      << "Editor configuration:\n"
      << "  .clangd is not created, changed or removed by sync. The project template\n"
      << "  creates it with CompileFlags: CompilationDatabase: . so clangd uses the\n"
      << "  compilation database copied to the project root.\n\n"
      << "Usage:\n  frankenstein sync\n";
    return;
  }

  if (command == "build" || command == "check" || command == "test") {
    std::cout
      << (command == "build" ? "Configure and compile the current project.\n\n" :
          command == "check" ? "Configure and build the project as a compilation check.\n\n" :
                                "Build the project and run its CTest suite.\n\n")
      << "Uses build/Debug by default. Pass --release to use build/Release and --jobs\n"
      << "to select parallel build jobs. The compilation database is exported by CMake\n"
      << "and copied to the project root for clangd.\n\n"
      << "Options:\n  --release       Use the Release configuration\n"
      << "  --debug         Use the Debug configuration\n"
      << "  --jobs <n>      Number of parallel build jobs\n"
      << "  --target <name> Build a specific target (build only)\n\n"
      << "Examples:\n  frankenstein " << command << "\n"
      << "  frankenstein " << command << " --release --jobs 8\n";
    return;
  }

  if (command == "publish") {
    std::cout
      << "Build the Release configuration and export the resulting executable.\n\n"
      << "Configures build/Release, builds the selected project target, and copies the\n"
      << "executable into publish/. The compilation database is also copied to the\n"
      << "project root. Review the output path printed when publishing completes.\n\n"
      << "Usage:\n  frankenstein publish\n";
    return;
  }

  if (command == "run") {
    std::cout
      << "Build the project when needed and run its executable.\n\n"
      << "Uses Debug by default; --release selects Release. Arguments after -- are\n"
      << "forwarded to the executable.\n\n"
      << "Usage:\n  frankenstein run [--release] [-- args...]\n\n"
      << "Example:\n  frankenstein run --release -- --verbose input.txt\n";
    return;
  }

  if (command == "clean") {
    std::cout
      << "Remove generated build files.\n\n"
      << "Deletes build/ only. It does not remove publish/, external/, the manifest,\n"
      << "the lockfile, CMakeLists.txt, .clangd or compile_commands.json.\n\n"
      << "Usage:\n  frankenstein clean\n";
    return;
  }

  if (command == "update") {
    std::cout
      << "Update a dependency to the latest commit on the branch declared in the manifest.\n\n"
      << "The dependency must already be declared in Frankenstein.toml. The new resolved\n"
      << "commit and archive hash are recorded in Frankenstein.lock.\n\n"
      << "Usage:\n  frankenstein update <owner/repository>\n";
    return;
  }

  if (command == "tree") {
    std::cout
      << "Display the dependency declarations from Frankenstein.toml.\n\n"
      << "Shows each dependency, its configured repository and branch, and its optional\n"
      << "CMake target. This is a manifest view; use sync to install or reconcile packages.\n\n"
      << "Usage:\n  frankenstein tree\n";
    return;
  }

  if (command == "doctor") {
    std::cout
      << "Check whether the local build environment has the required tools.\n\n"
      << "Checks CMake, a C/C++ compiler and CTest. It also reports optional build\n"
      << "features, such as whether this binary was compiled with CPR support for\n"
      << "GitHub repository search and dependency downloads.\n\n"
      << "Usage:\n  frankenstein doctor\n";
    return;
  }

  if (command == "info") {
    std::cout
      << "Print the project's Frankenstein.toml manifest.\n\n"
      << "Displays the project settings, source list and dependency declarations.\n\n"
      << "Usage:\n  frankenstein info\n";
    return;
  }

  if (command == "search") {
    std::cout
      << "Search GitHub repositories.\n\n"
      << "Searches repositories by the supplied terms and displays the most relevant\n"
      << "matches, including their default branch, URL, description, stars, forks and\n"
      << "watchers. Requires a build compiled with CPR support.\n\n"
      << "Usage:\n  frankenstein search <query...>\n\n"
      << "Example:\n  frankenstein search c++ http server\n";
    return;
  }

  std::cout << "Unknown command: " << command << "\n\n";
  printCommandList();
  std::cout << "Use 'frankenstein help <command>' for command-specific help.\n";
}
