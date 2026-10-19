add_library(Telescope STATIC Telescope.cpp)
target_include_directories(Telescope PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_strict_warnings_level(Telescope)
