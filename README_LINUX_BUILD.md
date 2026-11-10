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

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install (optional)
sudo make install
```

## Running

```bash
# Run from build directory
./build/bin/tortoisegit

# Or install and run from anywhere
tortoisegit
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

### Phase 2 - Core Libraries (Next)
- Git operations layer
- Console tools
- Scintilla integration
- External library builds

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
- [ ] Core Git library port (Phase 2)
- [ ] Main GUI application (Phase 3)
- [ ] Shell extension plugins (Phase 4)
- [ ] Package builds (AppImage, Flatpak, Deb) (Phase 5)
