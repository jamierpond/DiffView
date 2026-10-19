add_library(TextBuffer STATIC TextBuffer.cpp)
target_include_directories(TextBuffer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_strict_warnings_level(TextBuffer)
