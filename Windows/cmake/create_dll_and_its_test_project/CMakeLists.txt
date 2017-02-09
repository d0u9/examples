cmake_minimum_required (VERSION 3.6)

project (create_dll_and_its_test_project)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories ("${PROJECT_SOURCE_DIR}/include")

file (GLOB SRCS_MAIN "src/*.cpp")
file (GLOB HDRS_MAIN "include/*.h")
file (GLOB DEFS_MAIN "src/*.def")

## include (GenerateExportHeader)

add_library (my_lib SHARED ${SRCS_MAIN} ${HDRS_MAIN} ${DEFS_MAIN})

# Uncomment the below line for setting unicode encoding as the default.
#target_compile_definitions (my_lib PRIVATE -D_UNICODE -DUNICODE)

# test_project
add_subdirectory ("${PROJECT_SOURCE_DIR}/test_project")

set_property (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT test_project)