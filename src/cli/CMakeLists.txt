# Command Line Interface

# Sources
SET(GRAVITY_SRC gravity.c)

# Executable
add_executable(gravity ${GRAVITY_SRC})
target_link_libraries(gravity gravityapi_s)
target_include_directories(gravity PUBLIC ${GRAVITY_INCLUDE_DIR})

# Install
install(TARGETS gravity ${GRAVITY_TARGETS}
        RUNTIME DESTINATION ${GRAVITY_INSTALL_RUNTIME_PATH}
        LIBRARY DESTINATION ${GRAVITY_INSTALL_LIB_PATH}
        ARCHIVE DESTINATION ${GRAVITY_INSTALL_LIB_STATIC_PATH})
