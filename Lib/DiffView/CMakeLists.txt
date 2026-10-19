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
        Hooks.cpp
        Inbox.cpp
        Keymap.cpp
        Listener.cpp
        Responder.cpp
        Review.cpp
        ReviewMarkdown.cpp
        Search.cpp
        Store.cpp
        StreamEvents.cpp
        Text.cpp
        Transcript.cpp)

if (APPLE)
    target_sources(DiffViewCore PRIVATE AgentProcess-macOS.mm Inbox-macOS.mm)
elseif (WIN32)
    target_sources(DiffViewCore PRIVATE AgentProcess-Windows.cpp Inbox-Windows.cpp)
endif ()

target_include_directories(DiffViewCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DiffViewCore PUBLIC eacp-core TextBuffer)
set_default_warnings_level(DiffViewCore)
