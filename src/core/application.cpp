#include "application.h"

#include <memory>

#include "../cli/cli.h"
#include "../cli/command_handler.h"
#include "../commands/new_command.h"
#include "../commands/init_command.h"
#include "../commands/build_command.h"
#include "../commands/run_command.h"
#include "../commands/dependency_command.h"
#include "../commands/diagnostics_command.h"
#include "../cli/ascii.h"
#include "../cli/console.h"
#include <iostream>
#include <string>
#include <utility>
#include <cstdio>
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
}

Application::Application(datatypes::Version version, std::string version_date)
  : version(version), version_date(std::move(version_date)) {
}

const datatypes::Version& Application::getVersion() const {
  return version;
}

int Application::run(int argc, char* argv[]) {
  CLI cli;

  if (!cli.parse(argc, argv)) {
    return 1;
  }
  const std::string color_mode = cli.hasFlag("--color") ? cli.getFlagValue("--color") : "auto";
  const bool colors = color_mode == "always" || (color_mode == "auto" && terminalSupportsColor());
  Console::setColorEnabled(colors);
  ascii::setColorEnabled(colors);

  if (cli.wantsVersion()) {
    std::cout << "frankenstein " << version << " (" << version_date << ")\n";
    return 0;
  }
  if (cli.wantsHelp() || cli.getCommand().empty()) {
    printHelp(!cli.getCommand().empty() ? cli.getCommand() : (cli.getArguments().empty() ? "" : cli.getArguments()[0]));
    return 0;
  }

  CommandHandler commandHandler;

  commandHandler.registerCommand("new", std::make_unique<NewCommand>());
  commandHandler.registerCommand("init", std::make_unique<InitCommand>());
  commandHandler.registerCommand("build", std::make_unique<BuildCommand>());
  commandHandler.registerCommand("b", std::make_unique<BuildCommand>());
  commandHandler.registerCommand("run", std::make_unique<RunCommand>());
  commandHandler.registerCommand("r", std::make_unique<RunCommand>());
  commandHandler.registerCommand("clean", std::make_unique<BuildCommand>(BuildCommand::Action::Clean));
  commandHandler.registerCommand("check", std::make_unique<BuildCommand>(BuildCommand::Action::Check));
  commandHandler.registerCommand("test", std::make_unique<BuildCommand>(BuildCommand::Action::Test));
  commandHandler.registerCommand("add", std::make_unique<DependencyCommand>(DependencyCommand::Action::Add));
  commandHandler.registerCommand("remove", std::make_unique<DependencyCommand>(DependencyCommand::Action::Remove));
  commandHandler.registerCommand("rm", std::make_unique<DependencyCommand>(DependencyCommand::Action::Remove));
  commandHandler.registerCommand("install", std::make_unique<DependencyCommand>(DependencyCommand::Action::Install));
  commandHandler.registerCommand("update", std::make_unique<DependencyCommand>(DependencyCommand::Action::Update));
  commandHandler.registerCommand("list", std::make_unique<DependencyCommand>(DependencyCommand::Action::List));
  commandHandler.registerCommand("doctor", std::make_unique<DiagnosticsCommand>(DiagnosticsCommand::Action::Doctor));
  commandHandler.registerCommand("info", std::make_unique<DiagnosticsCommand>(DiagnosticsCommand::Action::Info));
  commandHandler.registerCommand("search", std::make_unique<DiagnosticsCommand>(DiagnosticsCommand::Action::Search));

  const int result = commandHandler.execute(cli);
  if (result == 127) printHelp(cli.getCommand());
  return result;
}

void Application::printHelp(const std::string& command) const {
  if (!command.empty()) {
    std::cout << "Usage: frankenstein " << command << " [OPTIONS] [ARGS]\n\n";
    if (command == "add") {
      std::cout << "Add a GitHub dependency or a source file.\n\n"
            << "Usage: frankenstein add owner/repository[:branch]\n"
            << "       frankenstein add src/file.cpp\n"
            << "Example: frankenstein add libcpr/cpr:master\n"
            << "Example: frankenstein add src/files.cpp\n\n"
            << "Repository dependencies are resolved to a commit and recorded with the\n"
            << "archive SHA-256 in Frankenstein.lock. CMake libraries are added and linked\n"
            << "when Frankenstein can identify a target. Header-only or ambiguous libraries\n"
            << "may need manual CMake setup.\n\n"
            << "Source files are recorded in Frankenstein.toml under [sources].\n"
            << "CMake reads them through the generated frankenstein.sources.cmake.\n";
      return;
    }
    if (command == "install") {
      std::cout << "Install dependencies from Frankenstein.toml using pinned revisions\n"
            << "and archive hashes recorded in Frankenstein.lock.\n";
      return;
    }
    if (command == "update") {
      std::cout << "Resolve each dependency branch again, download its latest commit,\n"
            << "and refresh Frankenstein.lock.\n";
      return;
    }
    if (command == "list") {
      std::cout << "List dependencies declared in Frankenstein.toml.\n";
      return;
    }
    if (command == "remove" || command == "rm") {
      std::cout << "Remove a dependency or a source file.\n\n"
            << "Usage: frankenstein remove owner/repository\n"
            << "       frankenstein remove src/file.cpp\n\n"
            << "Repository dependencies are removed from external/, CMakeLists.txt,\n"
            << "Frankenstein.toml and Frankenstein.lock.\n\n"
            << "Source files are removed from Frankenstein.toml [sources] only;\n"
            << "the file itself stays on disk.\n";
      return;
    }
    else if (command == "new") std::cout << "Create a new C/C++ project\n";
    else if (command == "build") std::cout << "Build the current project (--debug, --release)\n";
    else if (command == "run") std::cout << "Build and run the current project\n";
    else std::cout << "Run the " << command << " command\n";
    std::cout << "\nOptions:\n  -h, --help  Print help\n";
    return;
  }
  ascii::drawLogo();
  std::cout << "C++ project and package manager\n\n"
        << "Usage: frankenstein [OPTIONS] [COMMAND]\n\n"
        << "Options:\n  -V, --version          Print version info and exit\n"
        << "  -h, --help             Print help\n"
        << "      --color <WHEN>     Coloring: auto, always, never\n\n"
        << "Commands:\n"
        << "    new       Create a new project\n"
        << "    init      Initialize an existing project\n"
        << "    add       Add a GitHub dependency or a source file\n"
        << "    remove    Remove a dependency or a source file\n"
        << "    install   Install dependencies from the manifest\n"
        << "    update    Update manifest dependencies\n"
        << "    list      List project dependencies\n"
        << "    doctor    Check required development tools\n"
        << "    info      Display project manifest\n"
        << "    search    Search GitHub repositories\n"
        << "    build, b  Compile the current project\n"
        << "    check     Configure and check the current project\n"
        << "    run, r    Build and run the current project\n"
        << "    test      Run project tests\n"
        << "    clean     Remove build artifacts\n"
        << "    help      Display command help\n\n"
        << "Sources are declared in Frankenstein.toml under [sources].\n"
        << "Frankenstein regenerates frankenstein.sources.cmake before every build,\n"
        << "so removing a source from the manifest also removes it from the build.\n"
        << "Dependencies use HTTPS ZIP archives; the resolved commit and SHA-256\n"
        << "are recorded in Frankenstein.lock and verified on install.\n\n"
        << "See 'frankenstein help <command>' for more information.\n";
}