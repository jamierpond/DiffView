add_library(DiffViewCore STATIC
        Activity.cpp
        AgentCli.cpp
        Annotations.cpp
        ChangeTracker.cpp
        ClaudeCodeCli.cpp
        Diff.cpp
        DiffRows.cpp
        FileTree.cpp
        GeminiCli.cpp
        Git.cpp
        Keymap.cpp
        Responder.cpp
        Review.cpp
        ReviewMarkdown.cpp
        Search.cpp
        Store.cpp
        StreamEvents.cpp
        Text.cpp)

target_include_directories(DiffViewCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DiffViewCore PUBLIC eacp-core TextBuffer)
set_default_warnings_level(DiffViewCore)
