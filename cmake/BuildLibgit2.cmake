# Resolves the libgit2 dependency for the Linux port.
#
# Prefers a system-installed libgit2 (CMake package config) and falls back to
# the bundled submodule in ext/libgit2. Both branches provide the
# `tgit::libgit2` interface target that carries the library and the include
# directories required to use <git2.h>.

include_guard(GLOBAL)

if(TARGET tgit::libgit2)
    return()
endif()

# 1) System package (libgit2-dev / libgit2-devel)
find_package(libgit2 CONFIG QUIET)
if(TARGET libgit2::libgit2package)
    message(STATUS "Using system libgit2 (libgit2::libgit2package)")
    add_library(tgit_libgit2 INTERFACE)
    target_link_libraries(tgit_libgit2 INTERFACE libgit2::libgit2package)
    add_library(tgit::libgit2 ALIAS tgit_libgit2)
    return()
elseif(TARGET libgit2::git2)
    # Some distro packages export this target name instead
    message(STATUS "Using system libgit2 (libgit2::git2)")
    add_library(tgit_libgit2 INTERFACE)
    target_link_libraries(tgit_libgit2 INTERFACE libgit2::git2)
    add_library(tgit::libgit2 ALIAS tgit_libgit2)
    return()
endif()

# 2) Bundled submodule (ext/libgit2)
set(_tgit_libgit2_src "${CMAKE_CURRENT_LIST_DIR}/../ext/libgit2")
if(NOT EXISTS "${_tgit_libgit2_src}/CMakeLists.txt")
    message(FATAL_ERROR
        "libgit2 was not found. Either install the libgit2 development package\n"
        "or initialize the bundled submodule:\n"
        "    git submodule update --init ext/libgit2")
endif()

message(STATUS "Building bundled libgit2 from ${_tgit_libgit2_src}")

# Minimal, self-contained build: no network/auth backends in Phase 2 MVP,
# bundled regex/zlib so that no extra system packages are required.
set(BUILD_SHARED_LIBS OFF       CACHE BOOL   "" FORCE)
set(BUILD_TESTS OFF             CACHE BOOL   "" FORCE)
set(BUILD_CLI OFF               CACHE BOOL   "" FORCE)
set(BUILD_EXAMPLES OFF          CACHE BOOL   "" FORCE)
set(BUILD_FUZZERS OFF           CACHE BOOL   "" FORCE)
set(ENABLE_WERROR OFF           CACHE BOOL   "" FORCE)
set(EXPERIMENTAL_SHA256 OFF     CACHE BOOL   "" FORCE)
set(USE_SSH OFF                 CACHE STRING "" FORCE)
set(USE_HTTPS OFF               CACHE BOOL   "" FORCE)
set(USE_GSSAPI OFF              CACHE BOOL   "" FORCE)
set(USE_NTLMCLIENT OFF          CACHE BOOL   "" FORCE)
set(USE_ICONV OFF               CACHE BOOL   "" FORCE)
set(REGEX_BACKEND builtin       CACHE STRING "" FORCE)

# libgit2 runs its configure-time probes (FindStatNsec and friends) with
# CMAKE_C_STANDARD 90/CMAKE_C_EXTENSIONS OFF, i.e. strict -std=c90, where
# glibc hides POSIX members like st_mtim. It normally compiles everything
# with -D_GNU_SOURCE (cmake/DefaultCFlags.cmake), but that is applied only
# after the probes, so pass it through CMAKE_REQUIRED_DEFINITIONS (the
# check_* modules forward it to try_compile).
set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")

add_subdirectory("${_tgit_libgit2_src}" "${CMAKE_BINARY_DIR}/ext/libgit2" EXCLUDE_FROM_ALL)

unset(CMAKE_REQUIRED_DEFINITIONS)

if(NOT TARGET libgit2package)
    message(FATAL_ERROR "Bundled libgit2 did not define the 'libgit2package' target")
endif()
if(NOT TARGET libgit2::libgit2package)
    add_library(libgit2::libgit2package ALIAS libgit2package)
endif()

add_library(tgit_libgit2 INTERFACE)
target_link_libraries(tgit_libgit2 INTERFACE libgit2::libgit2package)
# git2.h includes the generated git2/experimental.h, which lives in the build tree
target_include_directories(tgit_libgit2 INTERFACE
    "${_tgit_libgit2_src}/include"
    "${CMAKE_BINARY_DIR}/ext/libgit2/include")
add_library(tgit::libgit2 ALIAS tgit_libgit2)
