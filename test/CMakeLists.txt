# Linux unit tests (Phase 2: gitcore layer).
# The original test/UnitTests project is Windows-only (vcxproj) and is not
# built here; new cross-platform tests live in test/gitcore.

# Build googletest from the bundled submodule without installing it.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/ext/googletest"
                 "${CMAKE_BINARY_DIR}/ext/googletest" EXCLUDE_FROM_ALL)

add_executable(gitcore_tests
    gitcore/RepositoryTest.cpp
    gitcore/LogTest.cpp
    gitcore/StatusTest.cpp
    gitcore/BranchesTest.cpp
    gitcore/DiffTest.cpp
    gitcore/CloneTest.cpp
)
target_link_libraries(gitcore_tests PRIVATE gitcore GTest::gtest_main)
target_include_directories(gitcore_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gitcore
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/testhome)

include(GoogleTest)
gtest_discover_tests(gitcore_tests
    PROPERTIES ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/testhome"
)
