add_library(gitdiffpivot_core STATIC
    git_diff_pivot/diff_input.cpp
    git_diff_pivot/line_normalization.cpp
    git_diff_pivot/token_interner.cpp
    git_diff_pivot/diff_document.cpp
    git_diff_pivot/unified_diff_parser.cpp
    git_diff_pivot/suffix_array_index.cpp
    git_diff_pivot/generalized_suffix_array_index.cpp
    git_diff_pivot/repeated_change_detector.cpp
    git_diff_pivot/change_selector.cpp
    git_diff_pivot/output_renderer.cpp
)

target_include_directories(gitdiffpivot_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# libsais is an implementation detail of suffix_array_index.cpp only.
target_link_libraries(gitdiffpivot_core PRIVATE libsais)

gitdiffpivot_set_warnings(gitdiffpivot_core)

# --- Product version, derived from git tags (see cmake/GitVersion.cmake) ---
gitdiffpivot_compute_version(GITDIFFPIVOT_VERSION)
message(STATUS "git-diff-pivot version: ${GITDIFFPIVOT_VERSION}")

configure_file(
    cli/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/cli/version.hpp
    @ONLY
)

# gitdiffpivot_cli holds CLI-only concerns (argument parsing, product version)
# that are not part of the reusable core algorithm library, kept as its own
# target so it stays independently testable without pulling in main.cpp.
add_library(gitdiffpivot_cli STATIC
    cli/cli_options.cpp
)

target_include_directories(gitdiffpivot_cli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(gitdiffpivot_cli PUBLIC gitdiffpivot_core)

gitdiffpivot_set_warnings(gitdiffpivot_cli)

add_executable(git-diff-pivot main.cpp)
target_link_libraries(git-diff-pivot PRIVATE gitdiffpivot_core gitdiffpivot_cli)

gitdiffpivot_set_warnings(git-diff-pivot)
