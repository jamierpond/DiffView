add_library(DiffViewCore STATIC
        Annotations.cpp
        ChangeTracker.cpp
        Diff.cpp
        DiffRows.cpp
        FileTree.cpp
        Git.cpp
        IntraLine.cpp
        Keymap.cpp
        Review.cpp
        ReviewMarkdown.cpp
        Search.cpp
        Store.cpp
        Text.cpp)

target_include_directories(DiffViewCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DiffViewCore PUBLIC eacp-core TextBuffer)
set_strict_warnings_level(DiffViewCore)
