cmake_minimum_required(VERSION 3.22.1)
project(BlackLangAndroid)

# Set the source directory to the root of the repo
set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

# Add the main project
add_subdirectory(${SRC_ROOT} blacklang_build)
