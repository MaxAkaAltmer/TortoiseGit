# Cross-platform build configuration
set(WIN_LIBS "")
if(WIN32)
    set(WIN_LIBS shlwapi version)
endif()

# === TortoiseGit Main Application ===
set(TGIT_SOURCES
    TortoiseGit/main.cpp
    TortoiseGit/MainWindow.cpp
    TortoiseGit/SettingsDialog.cpp
    TortoiseGit/LogDialog.cpp
    TortoiseGit/DiffDialog.cpp
    CrossPlatform/Path.cpp
    CrossPlatform/FileSystem.cpp
    CrossPlatform/Logging.cpp
)

set(TGIT_HEADERS
    TortoiseGit/main.h
    TortoiseGit/MainWindow.h
    TortoiseGit/SettingsDialog.h
    TortoiseGit/LogDialog.h
    TortoiseGit/DiffDialog.h
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
    target_link_libraries(tortoisegit PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
elseif(Qt5_FOUND)
    qt5_use_modules(tortoisegit Core Gui Widgets Network)
    target_link_libraries(tortoisegit PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
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

# === Build all external libraries ===
# External libraries will be built as static libraries

# === Configuration file ===
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/TortoiseGit/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)
target_include_directories(tortoisegit PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
