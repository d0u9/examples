include_directories ("${PROJECT_SOURCE_DIR}/include")

file (GLOB SRCS_EXPL "*.cpp")
file (GLOB HDRS_EXPL "*.h")

add_executable (test_project ${SRCS_EXPL} ${HDRS_EXPL})
target_link_libraries (test_project my_lib)