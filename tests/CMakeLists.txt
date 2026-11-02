# Unit tests. They must run without a GPU and without downloading a model.
# Tests that need the real weights are skipped unless EARSHOT_TEST_MODEL points
# at a ggml .bin file.

function(earshot_add_test name)
    add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/cli
        ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE earshot)
    if(TARGET earshot_cli_common)
        target_link_libraries(${name} PRIVATE earshot_cli_common)
    endif()
    earshot_apply_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "EARSHOT_TEST_MODEL=$ENV{EARSHOT_TEST_MODEL}")
endfunction()

file(GLOB EARSHOT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
foreach(source ${EARSHOT_TEST_SOURCES})
    get_filename_component(name "${source}" NAME_WE)
    earshot_add_test("${name}")
endforeach()
