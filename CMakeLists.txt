cmake_minimum_required(VERSION 3.25)

project(GitDiffPivot
    VERSION 0.1.0
    DESCRIPTION "Finds repeated changed-line sequences across a multi-file Git diff"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/utf-8)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(GitVersion)

include(FetchContent)

FetchContent_Declare(
    libsais
    GIT_REPOSITORY https://github.com/IlyaGrebnov/libsais.git
    GIT_TAG v2.10.4
    GIT_SHALLOW TRUE
)
set(LIBSAIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(LIBSAIS_USE_OPENMP OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libsais)

option(GITDIFFPIVOT_BUILD_TESTS "Build the git-diff-pivot test suite" ON)

add_subdirectory(src)

if (GITDIFFPIVOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
