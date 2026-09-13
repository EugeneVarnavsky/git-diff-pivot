# Applies the project's warning policy to a single target.
#
# Warnings are treated as errors for our own targets only. This function must
# never be applied to third-party targets, so their warnings can never break the build.
function(gitdiffpivot_set_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
