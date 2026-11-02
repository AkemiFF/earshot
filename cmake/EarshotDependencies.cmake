# ---------------------------------------------------------------------------
# whisper.cpp: system install or pinned FetchContent checkout
# ---------------------------------------------------------------------------

if(EARSHOT_USE_SYSTEM_WHISPER)
    find_package(whisper CONFIG REQUIRED)
    message(STATUS "earshot: using the system whisper.cpp")
else()
    include(FetchContent)

    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(whisper_cpp
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp
        GIT_TAG        ${EARSHOT_WHISPER_GIT_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)

    FetchContent_MakeAvailable(whisper_cpp)
    # whisper.cpp disables install rules for its standalone examples. That
    # cache setting must not leak into the parent project: earshot itself has
    # install rules for the library, headers, CLI and CMake package config.
    set(CMAKE_SKIP_INSTALL_RULES OFF CACHE BOOL "" FORCE)
    message(STATUS "earshot: whisper.cpp ${EARSHOT_WHISPER_GIT_TAG} fetched")
endif()

if(NOT TARGET whisper)
    message(FATAL_ERROR "earshot: the whisper target is missing (check whisper.cpp installation)")
endif()
