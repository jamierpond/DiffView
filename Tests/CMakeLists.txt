# One test binary per library under Lib/.
add_executable(DiffViewTests
        AnnotationsTests.cpp
        ChangeTrackerTests.cpp
        DiffRowsTests.cpp
        DiffTests.cpp
        FileTreeTests.cpp
        GitTests.cpp
        KeymapTests.cpp
        ReviewMarkdownTests.cpp
        ReviewTests.cpp
        SearchTests.cpp
        StoreTests.cpp
        TextTests.cpp)
target_link_libraries(DiffViewTests PRIVATE NanoTest NanoTestMain DiffViewCore)
target_compile_definitions(DiffViewTests PRIVATE
        DIFFVIEW_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Golden")
set_strict_warnings_level(DiffViewTests)
nano_discover_tests(DiffViewTests)

add_executable(TextBufferTests TextBufferTests.cpp)
target_link_libraries(TextBufferTests PRIVATE NanoTest NanoTestMain TextBuffer)
set_strict_warnings_level(TextBufferTests)
nano_discover_tests(TextBufferTests)

add_executable(VimEditorTests VimEditorTests.cpp)
target_link_libraries(VimEditorTests PRIVATE NanoTest NanoTestMain VimEditor)
set_strict_warnings_level(VimEditorTests)
nano_discover_tests(VimEditorTests)

add_executable(TelescopeTests TelescopeTests.cpp)
target_link_libraries(TelescopeTests PRIVATE NanoTest NanoTestMain Telescope)
set_strict_warnings_level(TelescopeTests)
nano_discover_tests(TelescopeTests)
