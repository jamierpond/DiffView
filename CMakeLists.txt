cmake_minimum_required(VERSION 3.31)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(DiffView VERSION 0.1.0 LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJCXX)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

# Reflection under the review Markdown. eacp pulls Miro at main; declaring
# it first pins the copy both of us use.
CPMAddPackage(
        NAME Miro
        GITHUB_REPOSITORY eyalamirmusic/Miro
        GIT_TAG 996af8bcfe68d8c707960663c1447197ff5dd5b9
        SYSTEM YES)

# Only eacp's Core is used, so its graphics stack stays unbuilt. Work against
# a local eacp checkout with:
#   cmake -B build -DCPM_eacp_SOURCE=$HOME/projects/eacp
CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG b8081e15d88b8cc3031c89bd676deb5db20a038f
        SYSTEM YES
        OPTIONS
            "EACP_BUILD_GRAPHICS OFF"
            "EACP_ENABLE_EXAMPLES OFF"
            "EACP_ENABLE_TESTS OFF")

list(APPEND CMAKE_MODULE_PATH "${eacp_SOURCE_DIR}/CMake")
include(cmake/StrictWarnings.cmake)

# eacp has no terminal UI. FTXUI's own loop is not used: eacp's timer pumps it.
CPMAddPackage(
        NAME ftxui
        GITHUB_REPOSITORY ArthurSonzogni/FTXUI
        GIT_TAG v7.0.3
        SYSTEM YES
        OPTIONS
            "FTXUI_ENABLE_INSTALL OFF"
            "FTXUI_QUIET ON")

add_subdirectory(Lib)

add_executable(DiffView
        Source/Agents.cpp
        Source/Commands.cpp
        Source/Comments.cpp
        Source/Input.cpp
        Source/Main.cpp
        Source/Monitor.cpp
        Source/Render.cpp
        Source/Viewer.cpp)

set_target_properties(DiffView PROPERTIES OUTPUT_NAME diffview)

# Elsewhere the repo is polled and only text pastes.
if (APPLE)
    target_sources(DiffView PRIVATE
            Source/Clipboard-macOS.mm
            Source/RepoWatcher-macOS.mm)
    target_link_libraries(DiffView PRIVATE
            "-framework AppKit"
            "-framework CoreServices")

    # eacp's loop is NSApplication's. An unbundled binary takes its Info.plist
    # from this section; LSBackgroundOnly keeps a terminal app out of the Dock.
    target_link_options(DiffView PRIVATE
            "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Source/Info.plist")
else ()
    target_sources(DiffView PRIVATE
            Source/Clipboard-Other.cpp
            Source/RepoWatcher-Poll.cpp)
endif ()

target_link_libraries(DiffView PRIVATE
        DiffViewCore
        VimEditor
        Telescope
        ftxui::screen
        ftxui::dom
        ftxui::component)
set_strict_warnings_level(DiffView)

if (NOT TARGET NanoTest)
    CPMAddPackage(
            NAME NanoTest
            GITHUB_REPOSITORY eyalamirmusic/NanoTest
            GIT_TAG main
            SYSTEM YES)
endif ()

enable_testing()
add_subdirectory(Tests)
