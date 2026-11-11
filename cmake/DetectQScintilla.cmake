# Resolves the optional QScintilla dependency of the diff viewer.
#
# TortoiseGit on Windows builds Scintilla from ext/scintilla, but that copy has
# no Qt platform layer (only win32/), so the Linux port uses the packaged
# QScintilla - the Qt front end of Scintilla - instead. When it is available the
# interface target `tgit::qscintilla` is defined and src/CMakeLists.txt compiles
# the Scintilla backend of DiffViewerWidget; otherwise the Qt fallback is used.
#
# TGIT_WITH_SCINTILLA:  AUTO - use QScintilla when it is found (default)
#                       ON   - stop the configure step when it is missing
#                       OFF  - always build the Qt fallback
# TGIT_QSCINTILLA_ROOT: optional prefix (e.g. an extracted package or a custom
#                       install) that is searched in addition to the system paths

include_guard(GLOBAL)

if(TARGET tgit::qscintilla)
    return()
endif()

set(TGIT_WITH_SCINTILLA "AUTO" CACHE STRING
    "Use QScintilla for the diff viewer (AUTO/ON/OFF)")
set_property(CACHE TGIT_WITH_SCINTILLA PROPERTY STRINGS AUTO ON OFF)

set(TGIT_QSCINTILLA_ROOT "" CACHE PATH
    "Prefix to search for QScintilla in addition to the system paths")

if(TGIT_WITH_SCINTILLA STREQUAL "OFF")
    message(STATUS "QScintilla disabled (TGIT_WITH_SCINTILLA=OFF): the diff viewer uses the Qt widget")
    return()
endif()

# QScintilla ships no CMake package config. Distro packages install its headers
# next to the Qt module headers (so .../qt6/Qsci) and name the library either
# qscintilla2_qt6 (Debian/Ubuntu) or qscintilla2-qt6 (Fedora, Arch).
if(Qt6_FOUND)
    set(_tgit_qsci_qt Qt6)
    set(_tgit_qsci_names qscintilla2_qt6 qscintilla2-qt6 qscintilla-qt6)
else()
    set(_tgit_qsci_qt Qt5)
    set(_tgit_qsci_names qscintilla2_qt5 qscintilla2-qt5)
endif()

# The include directory of QtCore is where the Qsci/ directory of the distro
# packages lives.
get_target_property(_tgit_qt_core_include ${_tgit_qsci_qt}::Core INTERFACE_INCLUDE_DIRECTORIES)

find_path(TGIT_QSCINTILLA_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    HINTS
        ${_tgit_qt_core_include}
        ${TGIT_QSCINTILLA_ROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}/qt6
        ${TGIT_QSCINTILLA_ROOT}/usr/include/qt6
        ${TGIT_QSCINTILLA_ROOT}/usr/include
        ${TGIT_QSCINTILLA_ROOT}/include
    PATH_SUFFIXES qt6 qt5)

find_library(TGIT_QSCINTILLA_LIBRARY
    NAMES ${_tgit_qsci_names}
    HINTS
        ${TGIT_QSCINTILLA_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
        ${TGIT_QSCINTILLA_ROOT}/usr/lib
        ${TGIT_QSCINTILLA_ROOT}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TgitQScintilla
    REQUIRED_VARS TGIT_QSCINTILLA_INCLUDE_DIR TGIT_QSCINTILLA_LIBRARY)

if(NOT TgitQScintilla_FOUND)
    if(TGIT_WITH_SCINTILLA STREQUAL "ON")
        message(FATAL_ERROR
            "TGIT_WITH_SCINTILLA=ON but QScintilla was not found.\n"
            "Install it (Debian/Ubuntu: libqscintilla2-qt6-dev, Fedora: "
            "qscintilla-qt6-devel, Arch: qscintilla-qt6) or point "
            "TGIT_QSCINTILLA_ROOT to a prefix containing Qsci/ and the library.")
    endif()
    message(STATUS "QScintilla not found: the diff viewer uses the Qt widget "
                   "(install libqscintilla2-qt6-dev for the Scintilla widget)")
    return()
endif()

add_library(tgit_qscintilla INTERFACE)
target_link_libraries(tgit_qscintilla INTERFACE
    ${_tgit_qsci_qt}::Core ${_tgit_qsci_qt}::Gui ${_tgit_qsci_qt}::Widgets
    ${TGIT_QSCINTILLA_LIBRARY})
target_include_directories(tgit_qscintilla INTERFACE ${TGIT_QSCINTILLA_INCLUDE_DIR})
add_library(tgit::qscintilla ALIAS tgit_qscintilla)

message(STATUS "Using QScintilla for the diff viewer: ${TGIT_QSCINTILLA_LIBRARY}")
