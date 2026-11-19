# Frankenstein

Frankenstein is a C/C++ project manager that uses CMake to configure and build projects. It can create projects, build and run them, and manage GitHub dependencies downloaded as HTTPS ZIP archives.

## Install a release

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

## Commands

```sh
frankenstein --help
frankenstein new my-project
frankenstein init
frankenstein build --release
frankenstein run
frankenstein add libcpr/cpr:master
frankenstein install
```

`add owner/repository[:branch]` resolves the requested branch to a commit, downloads its ZIP archive into `external/`, records its commit and SHA-256 in `Frankenstein.lock`, and adds a CMake integration when it can identify a library target. Some projects need manual CMake configuration; inspect the output from `add` and `cmake --help` when no unique target is detected.

Interactive `new` starts with `my-project` and selects the next available name (`my-project-2`, `my-project-3`, and so on) when that directory already exists. While entering a name, the gray default text is replaced by the brighter typed value. `build` and `run` read the standard from `[project].standard` in `Frankenstein.toml` and synchronize it to `CMakeLists.txt` and `.clang-format`. `run` reuses the existing executable when the project inputs and CMake configuration have not changed; otherwise it builds first.

## Releases

Pushing a version tag such as `v1.0.6` starts `.github/workflows/release.yml`. GitHub Actions builds Linux, macOS, and Windows archives, calculates `SHA256SUMS`, and publishes the assets as a GitHub Release. The install scripts download those release assets. A tag by itself is not enough: the workflow must finish and publish the Release before the `releases/latest/download` installer URL can work.

The Linux archive is built on Ubuntu 22.04 and targets compatible x86-64 and ARM64 Linux systems. CPR and its bundled curl are linked statically so the release archive does not require separate CPR or curl library files. Release builds require GitHub Actions network access to fetch the CPR curl dependency. The next release tag is `v1.0.6`.
