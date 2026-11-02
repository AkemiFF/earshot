#include <cassert>
#include <cstring>

#include "earshot/earshot.h"

int main() {
    assert(std::strcmp(earshot_version(), "0.1.0") == 0);
    assert(earshot_gpu_device_count() >= 0);
    const char * const * formats = earshot_supported_formats();
    assert(formats != nullptr && formats[0] != nullptr);

    char error[EARSHOT_ERROR_BUFFER_SIZE] = {};
    earshot_options options = earshot_default_options();
    options.model_path = "/definitely/not/a/model.bin";
    assert(earshot_create(&options, error, sizeof(error)) == nullptr);
    assert(std::strstr(error, "does not exist") != nullptr);

    std::memset(error, 'x', sizeof(error));
    assert(!earshot_probe_audio("/definitely/not/audio.mp3", nullptr, error, sizeof(error)));
    assert(std::strlen(error) > 0);
    return 0;
}
