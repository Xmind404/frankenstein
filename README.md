# Frankenstein

Frankenstein is a C/C++ project and package manager. It creates projects, builds and runs them, manages GitHub dependencies downloaded as HTTPS ZIP archives, and keeps `CMakeLists.txt` in sync with a single manifest file (`Frankenstein.toml`).

## Download

Linux and macOS:

```sh
curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/Xmind404/frankenstein/main/install.sh | bash
```

Windows PowerShell:

```powershell
irm https://raw.githubusercontent.com/Xmind404/frankenstein/main/install.ps1 | iex
```

The installer shows an interactive menu with arrow-key navigation. It offers **Install** when Frankenstein is not present, and **Reinstall**, **Uninstall**, or **Exit** when it is already installed. Confirm the selected action with `Y` or cancel with `N`. Downloads are verified against the release's SHA-256 checksums before the existing binary is replaced.

After installation, the installer asks whether to add its directory to PATH (`Y`/`N`). Choosing `Y` updates the current user's shell startup file on Linux/macOS or user PATH on Windows. Choosing `N` prints manual instructions. Linux/macOS defaults to `~/.local/bin`; Windows defaults to `%LOCALAPPDATA%\Frankenstein\bin`. Set `FRANKENSTEIN_INSTALL_DIR` before running the installer to choose another location. Open a new terminal after changing PATH.

To preselect the action in a terminal, set `FRANKENSTEIN_ACTION=install`, `reinstall`, or `uninstall`. The installer still asks for Y/N confirmation and asks whether to update PATH after installation.

## Build from source

Requirements: CMake 3.24 or later, a C++20 compiler, OpenSSL development files, and internet access during the first CMake configure to fetch CPR's bundled curl sources.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

## Quick start

```sh
frankenstein new my-project --template CXX20
cd my-project
frankenstein sync
frankenstein build
frankenstein run
```

`sync` is the command that makes `CMakeLists.txt`, `external/` and `Frankenstein.lock` match `Frankenstein.toml`. Run it after creating a project, after editing the manifest, and after adding or removing dependencies or sources.

## Commands

All commands must be run from the project root (the directory that contains `CMakeLists.txt` and `Frankenstein.toml`).

### Project creation

- `frankenstein new [name]` creates a new project in a new directory. Without a name it prompts for one (default `my-project`, then `my-project-2`, `my-project-3`, ...). `--template <name>` picks a template without prompting.
- `frankenstein init` initializes the **current** directory as a project, like `git init`. It adds the project skeleton without creating a new directory and without overwriting existing files. `--template <name>` works the same way as for `new`.

### Dependencies and sources

- `frankenstein add <owner/repository[:branch]>` downloads a GitHub branch as a ZIP archive, verifies its SHA-256, extracts it into `external/<name>/`, detects the CMake library targets it exposes, and asks you to pick one (or type one in manually). The chosen target is recorded in `Frankenstein.toml` under `[dependencies.<name>]`.
  - `--target <name>` skips target detection and uses this CMake target name directly.
  - `--rev <sha>` pins a specific commit (40 or 64 hex characters). Advanced.
  - `--sha256 <hex>` requires the downloaded archive to match this SHA-256. Advanced.
- `frankenstein add <src/file.cpp>` records a source file under `[sources].files` in `Frankenstein.toml`. The file must already exist inside the project root.
- `frankenstein remove <owner/repository>` removes the dependency from `Frankenstein.toml` and deletes `external/<name>/` from disk.
- `frankenstein remove <src/file.cpp>` (alias `rm`) removes the source file from `[sources].files`. The file itself is not deleted from disk.
- `frankenstein update <owner/repository>` re-downloads an already installed dependency at the tip of the branch stored in `Frankenstein.toml`. The dependency must already be tracked and installed.
- `frankenstein sync` synchronizes the project with `Frankenstein.toml`. It is the single command that makes `CMakeLists.txt`, `external/` and `Frankenstein.lock` match the manifest:
  1. Synchronizes `[project].standard` into `CMakeLists.txt` and `.clang-format`.
  2. Rewrites the `# BEGIN FRANKENSTEIN SOURCES` block in `CMakeLists.txt` from `[sources].files`.
  3. Removes any directory in `external/` that is not listed under `[dependencies.<name>]`.
  4. Downloads any dependency that is missing or out of date.
  5. Rewrites the `# BEGIN FRANKENSTEIN DEPENDENCY` blocks in `CMakeLists.txt` from `[dependencies.<name>]`.
  6. Writes `Frankenstein.lock` from the downloaded commits and SHA-256 hashes.
  7. Configures CMake for the Debug configuration with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and copies `compile_commands.json` into the project root.
- `frankenstein tree` prints the project name and version followed by every dependency from `Frankenstein.toml`, its branch and its CMake target.

### Building and running

- `frankenstein build` configures CMake in `build/<configuration>` and runs `cmake --build` for the given configuration. Debug is the default. This command does not modify the manifest, the lockfile or `CMakeLists.txt`.
  - `--release` builds the Release configuration.
  - `--target <name>` builds a specific CMake target.
  - `--jobs <n>` sets the number of parallel build jobs.
- `frankenstein publish` builds the Release configuration, asks for a file name, and copies the resulting executable into `publish/` at the project root. The absolute path of the published file is printed at the end.
- `frankenstein check` configures CMake and builds the selected configuration without running tests or publishing. Useful as a quick "does it compile?" check. Debug is the default.
  - `--release` checks the Release configuration.
  - `--jobs <n>` sets the number of parallel build jobs.
- `frankenstein test` configures CMake, builds the selected configuration, and runs `ctest` in `build/<configuration>` with `--output-on-failure`. Debug is the default.
  - `--release` builds and runs tests in Release.
  - `--jobs <n>` sets the number of parallel build jobs.
- `frankenstein run` builds the project if the executable is missing or older than the project's sources, then runs it. Extra arguments are passed directly to the executable. Debug is the default.
  - `--release` builds and runs the Release configuration.
- `frankenstein clean` removes the `build/` directory. It does not remove `publish/`, `external/`, `Frankenstein.toml`, `Frankenstein.lock` or `CMakeLists.txt`.

### Diagnostics

- `frankenstein doctor` checks that `cmake`, a C/C++ compiler and `ctest` are available on `PATH`.
- `frankenstein info` prints `Frankenstein.toml` verbatim.
- `frankenstein search <query...>` fetches the 30 best GitHub matches for the query, sorts them by star count and prints the top 5 with their default branch, GitHub URL, description and star/fork/watch counts.

## Manifest (Frankenstein.toml)

`Frankenstein.toml` describes the project. It is the single source of truth for the C/C++ standard, the compiled source files, and the GitHub dependencies.

```toml
[project]
name = "my-project"
version = "0.1.0"
standard = "20"

[sources]
files = [
  "src/main.cpp",
  "src/files.cpp",
]

[dependencies.webview]
repository = "webview/webview"
branch = "master"
target = "webview_core_static"
```

- `[project].name` — project name.
- `[project].version` — project version.
- `[project].standard` — C or C++ standard, synchronized into `CMakeLists.txt` and `.clang-format` by `frankenstein sync`.
- `[sources].files` — source files compiled into the project, mirrored into the `# BEGIN FRANKENSTEIN SOURCES` block in `CMakeLists.txt` by `frankenstein sync`.
- `[dependencies.<name>].repository` — `owner/repository` on GitHub.
- `[dependencies.<name>].branch` — branch to resolve when downloading.
- `[dependencies.<name>].target` — CMake library target to link against, detected by `frankenstein add` or set manually.

## Lockfile (Frankenstein.lock)

`Frankenstein.lock` is written by `frankenstein sync`. For every installed dependency it records:

```
repository|branch|commit|sha256
```

This makes builds reproducible and lets `frankenstein sync` re-download the exact same commit and verify the archive with SHA-256. Do not edit it by hand.

## compile_commands.json

`frankenstein sync` configures CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and copies `compile_commands.json` into the project root, where clangd reads it. `frankenstein build` also copies it after configuring CMake. `.clangd` in the project skeleton points at the project root for that reason.

## Releases

Pushing a version tag such as `v1.1.1` starts `.github/workflows/release.yml`. GitHub Actions builds Linux, macOS, and Windows archives, calculates `SHA256SUMS`, and publishes the assets as a GitHub Release. The install scripts download those release assets. A tag by itself is not enough: the workflow must finish and publish the Release before the `releases/latest/download` installer URL can work.

The Linux archive is built on Ubuntu 22.04 and targets compatible x86-64 and ARM64 Linux systems. CPR and its bundled curl are linked statically so the release archive does not require separate CPR or curl library files. Release builds require GitHub Actions network access to fetch the CPR curl dependency. The next release tag is `v1.1.1`.