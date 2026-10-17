add_library(VimEditor STATIC VimEditor.cpp)
target_include_directories(VimEditor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VimEditor PUBLIC TextBuffer)
set_default_warnings_level(VimEditor)
