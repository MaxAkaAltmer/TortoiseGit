# Cross-platform build configuration
set(WIN_LIBS "")
if(WIN32)
    set(WIN_LIBS shlwapi version)
endif()

# Run moc automatically for classes with Q_OBJECT
set(CMAKE_AUTOMOC ON)

# === TortoiseGit Main Application ===
set(TGIT_SOURCES
    TortoiseGit/main.cpp
    TortoiseGit/MainWindow.cpp
    TortoiseGit/SingleInstance.cpp
    TortoiseGit/SettingsDialog.cpp
    TortoiseGit/LogDialog.cpp
    TortoiseGit/StatusDialog.cpp
    TortoiseGit/DiffDialog.cpp
    TortoiseGit/DiffViewerWidget.cpp
    TortoiseGit/AppSettings.cpp
    CrossPlatform/Path.cpp
    CrossPlatform/FileSystem.cpp
    CrossPlatform/Logging.cpp
    CrossPlatform/ConfigStore.cpp
)

set(TGIT_HEADERS
    TortoiseGit/MainWindow.h
    TortoiseGit/SingleInstance.h
    TortoiseGit/SettingsDialog.h
    TortoiseGit/LogDialog.h
    TortoiseGit/StatusDialog.h
    TortoiseGit/DiffDialog.h
    TortoiseGit/DiffViewerWidget.h
    TortoiseGit/AppSettings.h
    CrossPlatform/Path.h
    CrossPlatform/ConfigStore.h
    CrossPlatform/Logging.h
    CrossPlatform/Thread.h
    CrossPlatform/FileSystem.h
    TortoiseGit/Commands/ICommand.h
)

add_executable(tortoisegit WIN32 MACOSX_BUNDLE
    ${TGIT_SOURCES}
    ${TGIT_HEADERS}
)

# Qt integration
if(Qt6_FOUND)
    qt_add_resources(tortoisegit "resources"
        PREFIX "/"
        FILES ""
    )
    target_link_libraries(tortoisegit PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent)
elseif(Qt5_FOUND)
    qt5_use_modules(tortoisegit Core Gui Widgets Network Concurrent)
    target_link_libraries(tortoisegit PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Concurrent)
endif()

# Phase 3: the GUI dialogs use the cross-platform git layer
target_link_libraries(tortoisegit PRIVATE gitcore)

# Optional: render patches with Scintilla instead of the Qt widget
include(DetectQScintilla)
if(TARGET tgit::qscintilla)
    target_link_libraries(tortoisegit PRIVATE tgit::qscintilla)
    target_compile_definitions(tortoisegit PRIVATE TGIT_HAVE_SCINTILLA)
endif()

# Optional: forward the command line of further instances over D-Bus
if(Qt6DBus_FOUND)
    target_link_libraries(tortoisegit PRIVATE Qt6::DBus)
    target_compile_definitions(tortoisegit PRIVATE TGIT_HAVE_DBUS)
elseif(Qt5DBus_FOUND)
    target_link_libraries(tortoisegit PRIVATE Qt5::DBus)
    target_compile_definitions(tortoisegit PRIVATE TGIT_HAVE_DBUS)
endif()

# Platform-specific linking
if(WIN32)
    target_link_libraries(tortoisegit PRIVATE ${WIN_LIBS})
elseif(APPLE)
    target_link_libraries(tortoisegit PRIVATE "-framework Foundation" "-framework AppKit")
else()
    # Linux: link necessary system libraries
    find_package(Threads REQUIRED)
    target_link_libraries(tortoisegit PRIVATE Threads::Threads)
endif()

# Include directories
target_include_directories(tortoisegit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/CrossPlatform
    ${CMAKE_CURRENT_SOURCE_DIR}/TortoiseGit
    ${CMAKE_CURRENT_SOURCE_DIR}/TortoiseGit/Commands
)

# Compile definitions for Linux
if(UNIX AND NOT APPLE)
    target_compile_definitions(tortoisegit PRIVATE
        LINUX
        _LINUX_
        HAVE_CONFIG_H
    )
endif()

# === Phase 2: cross-platform Git layer (no Qt, no GUI) ===
include(BuildLibgit2)

add_library(gitcore STATIC
    GitCore/GitException.cpp
    GitCore/Repository.cpp
    GitCore/Log.cpp
    GitCore/Status.cpp
    GitCore/Branches.cpp
    GitCore/Diff.cpp
    GitCore/Clone.cpp
)
target_include_directories(gitcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gitcore PUBLIC tgit::libgit2)

# === Phase 2: console utility ===
add_executable(tgit-cli
    tgit-cli/main.cpp
)
target_link_libraries(tgit-cli PRIVATE gitcore)
# nlohmann/json for the --json output
if(EXISTS "${CMAKE_SOURCE_DIR}/ext/json/include/nlohmann/json.hpp")
    target_include_directories(tgit-cli PRIVATE "${CMAKE_SOURCE_DIR}/ext/json/include")
endif()

# === Build all external libraries ===
# External libraries will be built as static libraries

# === Configuration file ===
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/TortoiseGit/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)
target_include_directories(tortoisegit PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
