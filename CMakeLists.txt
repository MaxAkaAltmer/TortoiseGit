cmake_minimum_required(VERSION 3.16)
# C is required because the bundled libgit2 runs C-only feature checks
# (check_struct_has_member ... LANGUAGE C) that fail in a CXX-only project.
project(TortoiseGit VERSION 3.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Our CMake modules (BuildLibgit2.cmake, ...)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Phase 2: unit tests for the gitcore layer
option(TGIT_BUILD_TESTS "Build gitcore unit tests" ON)
if(TGIT_BUILD_TESTS)
    enable_testing()
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

# Unit tests (googletest submodule)
if(TGIT_BUILD_TESTS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/ext/googletest/CMakeLists.txt")
        add_subdirectory(test)
    else()
        message(STATUS "googletest not checked out - skipping tests (git submodule update --init ext/googletest)")
    endif()
endif()

# Create installation targets
install(TARGETS tortoisegit tgit-cli DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/src/TortoiseGit.desktop DESTINATION share/applications)
if(EXISTS ${CMAKE_SOURCE_DIR}/resources)
    install(DIRECTORY resources DESTINATION share/tortoisegit)
endif()

# Package configuration
include(CPack)
