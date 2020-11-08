# Command Line Interface

# Sources
SET(GRAVITY_SRC gravity.c)

# Executable
add_executable(gravity ${GRAVITY_SRC})
target_link_libraries(gravity gravityapi_s)
target_include_directories(gravity PUBLIC ${GRAVITY_INCLUDE_DIR})
set_target_properties(gravity PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install
install(TARGETS gravity ${GRAVITY_TARGETS}
        RUNTIME DESTINATION ${GRAVITY_INSTALL_RUNTIME_PATH}
        LIBRARY DESTINATION ${GRAVITY_INSTALL_LIB_PATH}
        ARCHIVE DESTINATION ${GRAVITY_INSTALL_LIB_STATIC_PATH})
