# Library

include(GNUInstallDirs)

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

set(GRAVITY_INSTALL_RUNTIME_PATH ${CMAKE_INSTALL_BINDIR})     # Gravity executable install path
set(GRAVITY_INSTALL_LIB_PATH ${CMAKE_INSTALL_LIBDIR})         # Gravity shared library install path
set(GRAVITY_INSTALL_LIB_STATIC_PATH ${CMAKE_INSTALL_LIBDIR})  # Gravity static library install path
set(GRAVITY_INSTALL_INCLUDE_PATH ${CMAKE_INSTALL_INCLUDEDIR}) # Gravity headers install path

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

    if (WINDOWS_LOCAL_INSTALL)
    # make Windows installs local
    set(GRAVITY_INSTALL_RUNTIME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    set(GRAVITY_INSTALL_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
    set(GRAVITY_INSTALL_LIB_STATIC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
    set(GRAVITY_INSTALL_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/include")
    endif()

elseif(MINGW)

    # for path functions
    list(APPEND GRAVITY_DEPENDENT_LIBS "shlwapi")

    if (WINDOWS_LOCAL_INSTALL)
    # make Windows installs local
    set(GRAVITY_INSTALL_RUNTIME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    set(GRAVITY_INSTALL_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
    set(GRAVITY_INSTALL_LIB_STATIC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
    set(GRAVITY_INSTALL_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/include")
    endif()

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin|NetBSD|BSD|DragonFly|Linux")

    # for math functions
    list(APPEND GRAVITY_DEPENDENT_LIBS "m")

endif()

# ----------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    add_library(gravityapi SHARED ${SRC_FILES})
    target_compile_definitions(gravityapi PUBLIC BUILD_GRAVITY_API)
endif ()
add_library(gravityapi_s STATIC ${SRC_FILES})

# ----------------------------------------------------------------
if (BUILD_SHARED_LIBS)
    set(GRAVITY_TARGETS gravityapi gravityapi_s)
else()
    set(GRAVITY_TARGETS gravityapi_s)
endif()

foreach(target ${GRAVITY_TARGETS})

    target_link_libraries(${target} PRIVATE ${GRAVITY_DEPENDENT_LIBS})
    target_compile_definitions(${target} PRIVATE ${GRAVITY_PRIVATE_DEFINITIONS})
    target_compile_options(${target} PRIVATE ${GRAVITY_PRIVATE_COMPILE_OPTIONS})
    target_include_directories(${target} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${COMPILER_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${RUNTIME_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${SHARED_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${UTILS_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${OPT_DIR}>
            $<INSTALL_INTERFACE:${GRAVITY_INSTALL_INCLUDE_PATH}>)
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

endforeach()

# copy final install paths to parent scope for use in cli install
set(GRAVITY_INSTALL_RUNTIME_PATH    ${GRAVITY_INSTALL_RUNTIME_PATH}    PARENT_SCOPE)
set(GRAVITY_INSTALL_LIB_PATH        ${GRAVITY_INSTALL_LIB_PATH}        PARENT_SCOPE)
set(GRAVITY_INSTALL_LIB_STATIC_PATH ${GRAVITY_INSTALL_LIB_STATIC_PATH} PARENT_SCOPE)
set(GRAVITY_INSTALL_INCLUDE_PATH    ${GRAVITY_INSTALL_INCLUDE_PATH}    PARENT_SCOPE)

set(GRAVITYAPI_INSTALL_TARGETS ${GRAVITY_TARGETS})

file(GLOB GRAVITY_HEADERS
        "${COMPILER_DIR}/*.h" "${RUNTIME_DIR}/*.h" "${SHARED_DIR}/*.h" "${UTILS_DIR}/*.h" "${OPT_DIR}/*.h")
install(FILES ${GRAVITY_HEADERS}
        DESTINATION ${GRAVITY_INSTALL_INCLUDE_PATH})
install(TARGETS ${GRAVITYAPI_INSTALL_TARGETS}
        EXPORT gravity-targets
        ARCHIVE DESTINATION ${GRAVITY_INSTALL_LIB_STATIC_PATH}
        LIBRARY DESTINATION ${GRAVITY_INSTALL_LIB_PATH}
        RUNTIME DESTINATION ${GRAVITY_INSTALL_RUNTIME_PATH}
        BUNDLE  DESTINATION ${GRAVITY_INSTALL_RUNTIME_PATH})
install(EXPORT gravity-targets
        FILE gravity-config.cmake
        NAMESPACE gravity::
        DESTINATION ${GRAVITY_INSTALL_LIB_PATH}/cmake/gravity)
