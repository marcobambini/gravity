cmake_minimum_required(VERSION 3.10)
project(gravity VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

option(BUILD_CLI "Build the command line interface" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(WINDOWS_LOCAL_INSTALL "CMake install installs local to this checkout" ON)

# adds the BUILD_TESTING option (ON by default)
include(CTest)

# ----------------------------------------------------------------
# Library
add_subdirectory(src)

# ----------------------------------------------------------------
# Command Line Interface
if(BUILD_CLI)
    add_subdirectory(src/cli)
endif()

# ----------------------------------------------------------------
# Tests, run them with ctest
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
