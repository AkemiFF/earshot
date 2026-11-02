cmake_minimum_required(VERSION 3.20)

project(earshot
    VERSION 0.1.2
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

# CPack provides a portable binary Debian package for GitHub Releases. The
# source-package metadata under packaging/debian remains available for Debian
# maintainers who prefer debhelper.
set(CPACK_GENERATOR "DEB")
if(GGML_CUDA)
    set(CPACK_PACKAGE_NAME "earshot-cuda")
    set(CPACK_DEBIAN_PACKAGE_CONFLICTS "earshot")
    set(CPACK_DEBIAN_PACKAGE_REPLACES "earshot")
    # CUDA 12.6 from NVIDIA's APT repository: cudart and cuBLAS are linked by
    # ggml-cuda, but dpkg-shlibdeps cannot resolve libraries outside standard
    # system search paths. The driver is loaded separately at runtime.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libcuda1 | nvidia-driver")
    if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "12.6" AND
       CUDAToolkit_VERSION VERSION_LESS "12.7")
        string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", cuda-cudart-12-6, libcublas-12-6")
    else()
        message(WARNING "earshot: CUDA ${CUDAToolkit_VERSION} Debian runtime dependencies are unmapped; check-debs.sh will reject this package")
    endif()
else()
    set(CPACK_PACKAGE_NAME "earshot")
    set(CPACK_DEBIAN_PACKAGE_CONFLICTS "earshot-cuda")
    set(CPACK_DEBIAN_PACKAGE_REPLACES "earshot-cuda")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1")
endif()
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_PACKAGE_VENDOR "AkemiFF")
set(CPACK_PACKAGE_CONTACT "123939967+AkemiFF@users.noreply.github.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local speech-to-text with whisper.cpp")
set(CPACK_PACKAGE_DESCRIPTION "Offline speech-to-text library and CLI powered by whisper.cpp.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Model downloads invoke either tool when present; transcription works offline.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "curl | wget")
include(CPack)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/earshot" OPTIONAL)
