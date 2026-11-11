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

# --- Optional transports for authenticated remotes (Phase 3.4c) ----------
# The port asks the library at run time what it can actually do
# (git_libgit2_features(), see GitCore::SupportsHttps/SupportsSsh), so a
# missing backend here only limits the URLs the build can handle - it never
# breaks local or git:// repositories.
# TGIT_AUTH_ROOT points at an extra prefix with the headers and libraries,
# which is handy for a staged/extracted toolchain without installing packages.
option(TGIT_WITH_HTTPS "Build the bundled libgit2 with HTTPS (OpenSSL) support" ON)

set(TGIT_AUTH_ROOT "" CACHE PATH
    "Extra prefix containing the OpenSSL/libssh2 headers and libraries")

set(_tgit_auth_hints "${TGIT_AUTH_ROOT};${TGIT_AUTH_ROOT}/usr")
set(_tgit_auth_includes "include;usr/include;usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
set(_tgit_auth_libraries
    "lib;lib64;usr/lib;usr/lib64;usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")

set(_tgit_https OFF)
if(TGIT_WITH_HTTPS)
    find_package(OpenSSL QUIET HINTS ${_tgit_auth_hints}
                 PATHS ${_tgit_auth_hints} PATH_SUFFIXES ${_tgit_auth_includes})
    if(OPENSSL_FOUND AND OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
        set(_tgit_https OpenSSL)
        message(STATUS "libgit2 HTTPS transport: OpenSSL ${OPENSSL_VERSION}")
    else()
        message(STATUS "libgit2 HTTPS transport is disabled: OpenSSL development files not "
                       "found (install libssl-dev/libopenssl-devel or set TGIT_AUTH_ROOT)")
    endif()
endif()

# AUTO prefers libssh2 and falls back to the ssh client, LIBSSH2/EXEC choose
# one explicitly, OFF builds without any ssh transport.
set(TGIT_WITH_SSH "AUTO" CACHE STRING "SSH transport of libgit2: AUTO, LIBSSH2, EXEC or OFF")
set_property(CACHE TGIT_WITH_SSH PROPERTY STRINGS AUTO LIBSSH2 EXEC OFF)

set(_tgit_ssh OFF)
if(TGIT_WITH_SSH STREQUAL "AUTO" OR TGIT_WITH_SSH STREQUAL "LIBSSH2")
    find_path(LIBSSH2_INCLUDE_DIR NAMES libssh2.h
              HINTS ${_tgit_auth_hints} PATH_SUFFIXES ${_tgit_auth_includes})
    find_library(LIBSSH2_LIBRARY NAMES libssh2 ssh2
                 HINTS ${_tgit_auth_hints} PATH_SUFFIXES ${_tgit_auth_libraries})
    if(LIBSSH2_INCLUDE_DIR AND LIBSSH2_LIBRARY)
        set(_tgit_ssh libssh2)
        message(STATUS "libgit2 SSH transport: libssh2 ${LIBSSH2_LIBRARY}")
    endif()
endif()
if(NOT _tgit_ssh AND NOT TGIT_WITH_SSH STREQUAL "OFF" AND NOT TGIT_WITH_SSH STREQUAL "LIBSSH2")
    find_program(TGIT_SSH_PROGRAM ssh)
    if(TGIT_SSH_PROGRAM)
        # Uses the ssh client of the system: keys and ssh-agent keep working,
        # but a passphrase-protected key cannot be entered interactively.
        set(_tgit_ssh exec)
        message(STATUS "libgit2 SSH transport: the ssh client ${TGIT_SSH_PROGRAM} "
                       "(install libssh2-dev for a native transport)")
    endif()
endif()
if(_tgit_ssh STREQUAL "OFF")
    message(STATUS "libgit2 SSH transport is disabled (TGIT_WITH_SSH=OFF)")
endif()

# Minimal, self-contained build: bundled regex/zlib so that no extra system
# packages are required; the transports above are added when available.
set(BUILD_SHARED_LIBS OFF       CACHE BOOL   "" FORCE)
set(BUILD_TESTS OFF             CACHE BOOL   "" FORCE)
set(BUILD_CLI OFF               CACHE BOOL   "" FORCE)
set(BUILD_EXAMPLES OFF          CACHE BOOL   "" FORCE)
set(BUILD_FUZZERS OFF           CACHE BOOL   "" FORCE)
set(ENABLE_WERROR OFF           CACHE BOOL   "" FORCE)
set(EXPERIMENTAL_SHA256 OFF     CACHE BOOL   "" FORCE)
set(USE_SSH "${_tgit_ssh}"      CACHE STRING "" FORCE)
set(USE_HTTPS "${_tgit_https}"  CACHE STRING "" FORCE)
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
