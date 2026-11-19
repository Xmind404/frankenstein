# Frankenstein

Frankenstein is a C/C++ project manager that uses CMake to configure and build projects. It can create projects, build and run them, and manage GitHub dependencies downloaded as HTTPS ZIP archives.

## Install a release

Linux and macOS:

```sh
curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/Xmind404/frankenstein/main/install.sh | sh
```

Windows PowerShell:

```powershell
irm https://raw.githubusercontent.com/Xmind404/frankenstein/main/install.ps1 | iex
```

The installer downloads the matching archive from the latest GitHub Release, verifies its SHA-256 checksum, and installs it for the current user. On Linux and macOS it adds `~/.local/bin` (or the selected install directory) to the detected shell's startup file; on Windows it updates the current user's `Path`. The default locations are `~/.local/bin` and `%LOCALAPPDATA%\Frankenstein\bin`. Set `FRANKENSTEIN_INSTALL_DIR` to choose a different location. Open a new terminal after installation so it reads the updated environment variable.

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

## Releases

Pushing a version tag such as `v1.0.2` starts `.github/workflows/release.yml`. GitHub Actions builds Linux, macOS, and Windows archives, calculates `SHA256SUMS`, and publishes the assets as a GitHub Release. The install scripts download those release assets.

The Linux archive is built on Ubuntu 22.04 and targets compatible x86-64 and ARM64 Linux systems. Release builds require GitHub Actions network access to fetch the CPR curl dependency. The current release tag is `v1.0.2`.
