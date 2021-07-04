# Library

SET(COMPILER_DIR compiler/)
SET(RUNTIME_DIR runtime/)
SET(SHARED_DIR shared/)
SET(UTILS_DIR utils/)
SET(OPT_DIR optionals/)

AUX_SOURCE_DIRECTORY(${COMPILER_DIR} COMPILER_FILES)
AUX_SOURCE_DIRECTORY(${RUNTIME_DIR} RUNTIME_FILES)
AUX_SOURCE_DIRECTORY(${SHARED_DIR} SHARED_FILES)
AUX_SOURCE_DIRECTORY(${UTILS_DIR} UTILS_FILES)
AUX_SOURCE_DIRECTORY(${OPT_DIR} OPT_FILES)

set(GRAVITY_INCLUDE_DIR ${COMPILER_DIR} ${RUNTIME_DIR} ${SHARED_DIR} ${UTILS_DIR} ${OPT_DIR})

SET(SRC_FILES ${COMPILER_FILES} ${RUNTIME_FILES} ${SHARED_FILES} ${UTILS_FILES} ${OPT_FILES})

set(GRAVITY_DEPENDENT_LIBS "")
set(GRAVITY_PRIVATE_DEFINITIONS "")
set(GRAVITY_PRIVATE_COMPILE_OPTIONS "")

set(GRAVITY_INSTALL_RUNTIME_PATH "/usr/local/bin")  # Gravity executable install path
set(GRAVITY_INSTALL_LIB_PATH "lib")                 # Gravity shared library install path
set(GRAVITY_INSTALL_LIB_STATIC_PATH "lib")          # Gravity static library install path

# ----------------------------------------------------------------
if(MSVC)

    # for path functions
    list(APPEND GRAVITY_DEPENDENT_LIBS "shlwapi")

    # supress _CRT_SECURE_NO_WARNINGS for MSVC builds
    list(APPEND GRAVITY_PRIVATE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")

    # MSVC does not like static inlining
    list(APPEND GRAVITY_PRIVATE_DEFINITIONS "inline=")

    # warning C4068: unknown pragma
    list(APPEND GRAVITY_PRIVATE_COMPILE_OPTIONS "/wd4068")

    # make Windows installs local
    set(GRAVITY_INSTALL_RUNTIME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    set(GRAVITY_INSTALL_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
    set(GRAVITY_INSTALL_LIB_STATIC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")

elseif(MINGW)

    # for path functions
    list(APPEND GRAVITY_DEPENDENT_LIBS "shlwapi")

    # make Windows installs local
    set(GRAVITY_INSTALL_RUNTIME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    set(GRAVITY_INSTALL_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
    set(GRAVITY_INSTALL_LIB_STATIC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin|NetBSD|BSD|DragonFly|Linux")

    # for math functions
    list(APPEND GRAVITY_DEPENDENT_LIBS "m")

endif()

# ----------------------------------------------------------------
add_library(gravityapi SHARED ${SRC_FILES})
add_library(gravityapi_s STATIC ${SRC_FILES})

target_compile_definitions(gravityapi PUBLIC BUILD_GRAVITY_API)

# ----------------------------------------------------------------
set(GRAVITY_TARGETS gravityapi gravityapi_s)

foreach(target ${GRAVITY_TARGETS})

    target_link_libraries(${target} PRIVATE ${GRAVITY_DEPENDENT_LIBS})
    target_compile_definitions(${target} PRIVATE ${GRAVITY_PRIVATE_DEFINITIONS})
    target_compile_options(${target} PRIVATE ${GRAVITY_PRIVATE_COMPILE_OPTIONS})
    target_include_directories(${target} PUBLIC ${GRAVITY_INCLUDE_DIR})
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

endforeach()
