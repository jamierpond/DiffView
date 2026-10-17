add_library(DiffViewCore STATIC
        Annotations.cpp
        ChangeTracker.cpp
        Diff.cpp
        DiffRows.cpp
        FileTree.cpp
        Git.cpp
        Keymap.cpp
        Review.cpp
        ReviewMarkdown.cpp
        Search.cpp
        Text.cpp)

target_include_directories(DiffViewCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DiffViewCore PUBLIC eacp-core TextBuffer)
set_default_warnings_level(DiffViewCore)
