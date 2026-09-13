# Computes the product version from git tags at configure time, 
# derives a package version from git history: the nearest reachable "v<major>.<minor>.<patch>"
# tag is the base version, and any commits on top of that tag turn into a
# SemVer 2.0 pre-release/build-metadata suffix.
#
# Resulting version shape:
#   - Exact tag, clean working tree: "<major>.<minor>.<patch>"
#   - Otherwise:                     "<major>.<minor>.<patch>-dev.<height>+<shortHash>[.dirty]"
#   - No git repository, or no reachable "v*" tag (e.g. a source tarball or a
#     shallow checkout without tags): falls back to "${PROJECT_VERSION}+unknown"
#     so a version is always produced.
#
# gitdiffpivot_compute_version(<outVar>) sets <outVar> in the caller's scope.
function(gitdiffpivot_compute_version outVar)
    set(_fallback "${PROJECT_VERSION}+unknown")

    find_package(Git QUIET)
    if (NOT Git_FOUND OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
        set(${outVar} "${_fallback}" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --dirty=.dirty --match "v[0-9]*.[0-9]*.[0-9]*"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _describeResult
    )

    if (NOT _describeResult EQUAL 0 OR _describe STREQUAL "")
        set(${outVar} "${_fallback}" PARENT_SCOPE)
        return()
    endif()

    # _describe looks like "v0.1.0-5-gabcdef1" or "v0.1.0-5-gabcdef1.dirty".
    string(REGEX MATCH "^v([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g([0-9a-f]+)(\\.dirty)?$" _match "${_describe}")
    if (NOT _match)
        set(${outVar} "${_fallback}" PARENT_SCOPE)
        return()
    endif()

    set(_base "${CMAKE_MATCH_1}")
    set(_height "${CMAKE_MATCH_2}")
    set(_hash "${CMAKE_MATCH_3}")
    set(_dirty "${CMAKE_MATCH_4}")

    if (_height STREQUAL "0" AND _dirty STREQUAL "")
        set(${outVar} "${_base}" PARENT_SCOPE)
    else()
        set(_version "${_base}-dev.${_height}+${_hash}")
        if (NOT _dirty STREQUAL "")
            set(_version "${_version}.dirty")
        endif()
        set(${outVar} "${_version}" PARENT_SCOPE)
    endif()
endfunction()
