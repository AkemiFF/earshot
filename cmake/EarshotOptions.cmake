# ---------------------------------------------------------------------------
# User facing build options
# ---------------------------------------------------------------------------

option(EARSHOT_BUILD_CLI    "Build the earshot command line tool" ON)
option(EARSHOT_BUILD_TESTS  "Build the earshot test suite"        ON)
option(EARSHOT_CUDA         "Build with the CUDA backend when available" ON)
option(EARSHOT_USE_SYSTEM_WHISPER "Use an installed whisper.cpp instead of fetching it" OFF)
option(EARSHOT_INSTALL_MODELS "Install the default models alongside the binary" OFF)

set(EARSHOT_WHISPER_GIT_TAG "v1.9.5" CACHE STRING
    "whisper.cpp tag or commit fetched when EARSHOT_USE_SYSTEM_WHISPER is OFF")

set(EARSHOT_DEFAULT_MODEL "large-v3-turbo" CACHE STRING
    "Model used when neither --model nor the config file specifies one")

# Detect the CUDA toolkit unless the user explicitly disabled it.
if(EARSHOT_CUDA AND NOT DEFINED GGML_CUDA)
    find_program(EARSHOT_NVCC nvcc PATHS /opt/cuda/bin ENV CUDA_HOME PATH_SUFFIXES bin)
    if(EARSHOT_NVCC)
        set(GGML_CUDA ON CACHE BOOL "Build ggml with CUDA support" FORCE)
        message(STATUS "earshot: CUDA backend enabled (${EARSHOT_NVCC})")
    else()
        set(GGML_CUDA OFF CACHE BOOL "Build ggml with CUDA support" FORCE)
        message(STATUS "earshot: nvcc not found, building the CPU backend only")
    endif()
endif()
