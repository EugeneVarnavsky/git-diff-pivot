include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(gitdiffpivot_tests
    sanity_test.cpp
    line_normalization_test.cpp
    token_interner_test.cpp
    diff_document_test.cpp
    unified_diff_parser_test.cpp
    suffix_array_index_test.cpp
    generalized_suffix_array_index_test.cpp
    repeated_sequence_fixture_test.cpp
    repeated_change_detector_test.cpp
    change_selector_test.cpp
    output_renderer_test.cpp
    cli_options_test.cpp
    pipeline_integration_test.cpp
)

target_link_libraries(gitdiffpivot_tests PRIVATE gitdiffpivot_core gitdiffpivot_cli Catch2::Catch2WithMain)
gitdiffpivot_set_warnings(gitdiffpivot_tests)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(gitdiffpivot_tests)
