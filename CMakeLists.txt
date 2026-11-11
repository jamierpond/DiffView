cmake_minimum_required(VERSION 3.31)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/lockstep.cmake)

project(DiffView VERSION 0.1.0 LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJCXX)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# No source this project compiles uses C++20 modules, so scanning each one for them is wasted work.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Only eacp's Core is used, so its graphics stack stays unbuilt. eacp brings
# Miro, the reflection under the review Markdown; lockstep.lock pins both. Work against
# a local eacp checkout with:
#   cmake -B build -DLOCKSTEP_eacp_SOURCE=$HOME/projects/eacp
lockstep_add_package(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG develop
        SYSTEM YES
        OPTIONS
            "EACP_BUILD_GRAPHICS OFF"
            "EACP_ENABLE_EXAMPLES OFF"
            "EACP_ENABLE_TESTS OFF")

list(APPEND CMAKE_MODULE_PATH "${eacp_SOURCE_DIR}/CMake")
include(cmake/StrictWarnings.cmake)

# eacp has no terminal UI. FTXUI's own loop is not used: eacp's timer pumps it.
lockstep_add_package(
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
    lockstep_add_package(
            NAME NanoTest
            GITHUB_REPOSITORY eyalamirmusic/NanoTest
            GIT_TAG main
            SYSTEM YES)
endif ()

if (PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(Tests)
endif ()
