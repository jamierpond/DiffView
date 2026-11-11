# eacp's set_default_warnings_level plus the strict set, as errors. Only this
# repo's targets call it; dependencies come in as SYSTEM includes.
include(CheckCXXCompilerFlag)

function(set_strict_warnings_level target)
    set_default_warnings_level(${target})

    if (MSVC)
        target_compile_options(${target} PRIVATE /WX)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
                -Wshadow
                -Wconversion
                -Wsign-conversion
                -Wold-style-cast
                -Wunused
                -Wnon-virtual-dtor
                -Woverloaded-virtual
                -Wimplicit-fallthrough
                -Wextra-semi
                -Wdouble-promotion
                -Wcast-qual
                -Wnull-dereference
                -Wformat=2
                -Wmissing-declarations
                -Werror)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
        target_compile_options(${target} PRIVATE
                -Wshadow-all
                -Wunreachable-code-aggressive
                -Wunused-member-function
                -Wunused-template
                -Wunused-macros
                -Wcomma
                -Wdeprecated
                -Wundef
                -Wfloat-equal
                -Wextra-semi-stmt
                -Wzero-as-null-pointer-constant
                -Wimplicit-int-conversion)

        # clang 19+ flags `{.field = value}` that leaves other fields at their
        # defaults, which is how this repo writes its structs.
        check_cxx_compiler_flag(-Wno-missing-designated-field-initializers
                DIFFVIEW_HAS_MISSING_DESIGNATED_FIELD_INITIALIZERS_WARNING)
        if (DIFFVIEW_HAS_MISSING_DESIGNATED_FIELD_INITIALIZERS_WARNING)
            target_compile_options(${target} PRIVATE
                    -Wno-missing-designated-field-initializers)
        endif ()
    endif ()
endfunction()
