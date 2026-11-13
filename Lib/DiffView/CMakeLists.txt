add_library(DiffViewCore STATIC
        Annotations.cpp
        ChangeTracker.cpp
        Diff.cpp
        DiffRows.cpp
        FileTree.cpp
        Git.cpp
        Highlight.cpp
        IntraLine.cpp
        Keymap.cpp
        PathFilter.cpp
        Review.cpp
        ReviewMarkdown.cpp
        Search.cpp
        Store.cpp
        Text.cpp)

if (WIN32)
    target_sources(DiffViewCore PRIVATE Platform-Windows.cpp)
else ()
    target_sources(DiffViewCore PRIVATE Platform-Posix.cpp)
endif ()

target_include_directories(DiffViewCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DiffViewCore PUBLIC eacp-core TextBuffer)
set_strict_warnings_level(DiffViewCore)
