# Tests

# bounds tests for the JSON scanner, see loadbuffer/json_bounds.c
add_executable(jsontest loadbuffer/json_bounds.c)
target_link_libraries(jsontest gravityapi_s)
add_test(NAME jsontest COMMAND jsontest)

# checks the result declared in every #unittest block (gravity -t)
if(BUILD_CLI)
    add_test(NAME unittest COMMAND gravity -t ${CMAKE_CURRENT_SOURCE_DIR}/unittest)
endif()
