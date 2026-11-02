# ---------------------------------------------------------------------------
# User facing build options
# ---------------------------------------------------------------------------

option(EARSHOT_BUILD_CLI    "Build the earshot command line tool" ON)
option(EARSHOT_BUILD_TESTS  "Build the earshot test suite"        ON)
set(EARSHOT_CUDA AUTO CACHE STRING "CUDA backend: AUTO, ON (required), or OFF")
set_property(CACHE EARSHOT_CUDA PROPERTY STRINGS AUTO ON OFF)
option(EARSHOT_USE_SYSTEM_WHISPER "Use an installed whisper.cpp instead of fetching it" OFF)
option(EARSHOT_INSTALL_MODELS "Install the default models alongside the binary" OFF)

set(EARSHOT_WHISPER_GIT_TAG "v1.9.5" CACHE STRING
    "whisper.cpp tag or commit fetched when EARSHOT_USE_SYSTEM_WHISPER is OFF")

set(EARSHOT_DEFAULT_MODEL "large-v3-turbo" CACHE STRING
    "Model used when neither --model nor the config file specifies one")

# Resolve the backend on every configure: GGML_CUDA is a cached upstream option
# and must not keep a previous configuration's backend when EARSHOT_CUDA changes.
if(EARSHOT_CUDA STREQUAL "OFF")
    set(GGML_CUDA OFF CACHE BOOL "Build ggml with CUDA support" FORCE)
elseif(EARSHOT_CUDA STREQUAL "AUTO" OR EARSHOT_CUDA STREQUAL "ON")
    find_program(EARSHOT_NVCC nvcc PATHS /opt/cuda/bin ENV CUDA_HOME PATH_SUFFIXES bin)
    if(EARSHOT_NVCC)
        set(GGML_CUDA ON CACHE BOOL "Build ggml with CUDA support" FORCE)
        message(STATUS "earshot: CUDA backend enabled (${EARSHOT_NVCC})")
    else()
        set(GGML_CUDA OFF CACHE BOOL "Build ggml with CUDA support" FORCE)
        if(EARSHOT_CUDA STREQUAL "ON")
            message(FATAL_ERROR "earshot: EARSHOT_CUDA=ON was requested but the CUDA toolkit (nvcc) was not found")
        endif()
        message(STATUS "earshot: nvcc not found, building the CPU backend only")
    endif()
else()
    message(FATAL_ERROR "earshot: EARSHOT_CUDA must be AUTO, ON, or OFF (got ${EARSHOT_CUDA})")
endif()
