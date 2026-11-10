cmake_minimum_required(VERSION 3.16)
project(TortoiseGit VERSION 3.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find Qt6 (or Qt5 as fallback)
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
endif()

# Find libgit2
find_package(libgit2 QUIET)
if(NOT libgit2_FOUND)
    message(STATUS "libgit2 not found via find_package, will use bundled version")
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Platform-specific definitions
if(WIN32)
    add_definitions(-D_WIN32 -D_CRT_SECURE_NO_WARNINGS)
    # Link Windows libraries
    set(WIN_LIBS shell32 user32 gdi32 advapi32 shlwapi ole32 version ws2_32)
endif()

# Add source directories
add_subdirectory(src)

# Create installation targets
install(TARGETS tortoisegit DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/src/TortoiseGit.desktop DESTINATION share/applications)
install(DIRECTORY resources DESTINATION share/tortoisegit)

# Package configuration
include(CPack)
