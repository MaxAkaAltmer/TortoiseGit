# TortoiseGit Linux Build Instructions

## Prerequisites

### Ubuntu/Debian/Mint
```bash
sudo apt install \
    build-essential cmake git \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
    libqt6core6 libqt6gui6 libqt6widgets6 libqt6network6 \
    libgit2-dev \
    libhunspell-dev \
    libpcre2-dev \
    libssl-dev
```

### Fedora/RHEL
```bash
sudo dnf install \
    gcc-c++ cmake git \
    qt6-qtbase-devel qt6-qttools-devel \
    qt6-qtbase qt6-qttools \
    libgit2-devel \
    hunspell-devel \
    pcre2-devel \
    openssl-devel
```

### Arch Linux
```bash
sudo pacman -S \
    base-devel cmake git \
    qt6-base qt6-tools \
    libgit2 hunspell pcre2 openssl
```

## Building

```bash
# Clone and initialize submodules
git clone https://gitlab.com/tortoisegit/tortoisegit.git
cd tortoisegit
git submodule update --init

# Minimum submodules needed for a build (GUI + git layer + tests):
git submodule update --init ext/libgit2 ext/googletest ext/json

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Unit tests (gitcore, googletest-based)
ctest

# Install (optional)
sudo make install
```

CMake options:

| Option | Default | Meaning |
|--------|---------|---------|
| `TGIT_BUILD_TESTS` | `ON` | Build `gitcore_tests` and register them with CTest (requires `ext/googletest`) |

The git layer (`src/GitCore`) links against libgit2 resolved by
`cmake/BuildLibgit2.cmake`: a system package is used when available,
otherwise the bundled `ext/libgit2` submodule is built statically with
minimal features (no SSH/HTTPS/GSSAPI backends in the Phase 2 MVP).
`libgit2-dev` in the prerequisites above is therefore optional.

Note (porting): the top-level project enables both `C` and `CXX`, and
`BuildLibgit2.cmake` passes `-D_GNU_SOURCE` through
`CMAKE_REQUIRED_DEFINITIONS`. Without this, libgit2's configure-time
probes run in strict `-std=c90` mode, fail to detect `struct stat::st_mtim`
and the build breaks later with `GIT_USE_NSEC defined but unknown struct
stat nanosecond type`.

## Running

```bash
# Run from build directory
./build/bin/tortoisegit

# Or install and run from anywhere
tortoisegit
```

The console utility (Phase 2) does not need a display:

```bash
./build/bin/tgit-cli status
./build/bin/tgit-cli log -n 10 --json
./build/bin/tgit-cli diff HEAD~1 HEAD -- src/CrossPlatform/Path.cpp
./build/bin/tgit-cli --help
```

## Configuration

On Linux, TortoiseGit stores configuration in:
- **User config:** `~/.config/TortoiseGit/`
- **Logs:** `~/.local/share/TortoiseGit/logs/`

## Architecture

The Linux port follows a phased approach:

### Phase 1 - Infrastructure (Current)
- Cross-platform path utilities (`CrossPlatform/Path.h`)
- Config store replacement (`CrossPlatform/ConfigStore.h`)
- Logging system (`CrossPlatform/Logging.h`)
- Thread primitives (`CrossPlatform/Thread.h`)
- File system utilities (`CrossPlatform/FileSystem.h`)
- Qt-based application entry point

### Phase 2 - Core Libraries (complete)
- [x] Git operations layer (`src/GitCore`: libgit2 wrapper - repository
      discovery/init, log, status, branches, diff, clone, `safe.directory`)
- [x] Console tools (`src/tgit-cli`: init/clone/status/log/branches/show/diff/trust,
      human-readable and `--json` output)
- [x] Unit tests (`test/gitcore`, googletest + CTest)
Deferred to Phase 3+: Scintilla integration, additional external library
builds (hunspell/pcre2/etc.), network remotes with authentication (SSH/HTTPS),
wiring gitcore into the GUI dialogs.

### Phase 3 - GUI (Future)
- Qt-based main application
- Dialog porting
- Shell extension (Nautilus/Dolphin plugins)

## Key Design Decisions

1. **Qt6 as primary UI framework** - Replaces MFC for cross-platform compatibility
2. **XDG Base Directory spec** - Linux config follows XDG standards
3. **INI config files** - Replaces Windows Registry for settings
4. **CMake build system** - Replaces Visual Studio project files
5. **D-Bus for IPC** - Replaces Windows COM for inter-process communication

## Status

- [x] Infrastructure layer (Phase 1)
- [x] Core Git library port (Phase 2) - gitcore + tgit-cli, 25 unit tests;
      deferred to Phase 3+: network remotes with authentication (SSH/HTTPS),
      Scintilla, further libraries, GUI wiring
- [ ] Main GUI application (Phase 3)
- [ ] Shell extension plugins (Phase 4)
- [ ] Package builds (AppImage, Flatpak, Deb) (Phase 5)
