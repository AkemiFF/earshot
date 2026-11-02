cmake_minimum_required(VERSION 3.20)

project(earshot
    VERSION 0.1.1
    DESCRIPTION "Local speech-to-text, from the command line"
    HOMEPAGE_URL "https://github.com/akemiff/earshot"
    LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(EarshotOptions)
include(EarshotCompilerWarnings)
include(EarshotDependencies)

# ---------------------------------------------------------------------------
# core library (libearshot)
# ---------------------------------------------------------------------------
file(GLOB EARSHOT_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(earshot STATIC ${EARSHOT_CORE_SOURCES})
add_library(earshot::earshot ALIAS earshot)

target_include_directories(earshot
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_compile_definitions(earshot
    PRIVATE EARSHOT_VERSION="${PROJECT_VERSION}"
    PUBLIC  EARSHOT_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(earshot PRIVATE $<BUILD_INTERFACE:whisper>)
earshot_apply_warnings(earshot)

if(GGML_CUDA AND NOT MSVC)
    target_compile_definitions(earshot PRIVATE EARSHOT_HAVE_CUDA=1)
endif()

# ---------------------------------------------------------------------------
# command line interface
# ---------------------------------------------------------------------------
if(EARSHOT_BUILD_CLI)
    file(GLOB EARSHOT_CLI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cli/*.cpp")
    list(REMOVE_ITEM EARSHOT_CLI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cli/main.cpp")

    # Everything except main() lives in a small library so the unit tests can
    # exercise the argument parser, the config reader and the model catalogue.
    add_library(earshot_cli_common STATIC ${EARSHOT_CLI_SOURCES})
    target_include_directories(earshot_cli_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    target_link_libraries(earshot_cli_common PUBLIC earshot)
    earshot_apply_warnings(earshot_cli_common)

    add_executable(earshot_cli "${CMAKE_CURRENT_SOURCE_DIR}/cli/main.cpp")
    set_target_properties(earshot_cli PROPERTIES OUTPUT_NAME earshot)
    target_link_libraries(earshot_cli PRIVATE earshot_cli_common)
    earshot_apply_warnings(earshot_cli)

    install(TARGETS earshot_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/earshot.1"
            DESTINATION "${CMAKE_INSTALL_MANDIR}/man1" OPTIONAL)
endif()

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
if(EARSHOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# installation of the development files
# ---------------------------------------------------------------------------
install(TARGETS earshot EXPORT earshotTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/earshot"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT earshotTargets
        FILE earshotTargets.cmake
        NAMESPACE earshot::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/earshot")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/earshotConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/earshotConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/earshot")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/earshotConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/earshotConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/earshotConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/earshot")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/earshot" OPTIONAL)
