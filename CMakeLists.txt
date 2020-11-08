cmake_minimum_required(VERSION 3.0)
project(gravity VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# ----------------------------------------------------------------
# Library
add_subdirectory(src)

# ----------------------------------------------------------------
# Command Line Interface
add_subdirectory(src/cli)