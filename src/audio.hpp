#ifndef EARSHOT_AUDIO_HPP
#define EARSHOT_AUDIO_HPP

#include <string>
#include <vector>

#include "earshot/earshot.h"

namespace earshot::detail {

struct AudioBuffer {
    std::vector<float> samples;
    int64_t duration_ms = 0;
};

bool decode_audio(const std::string & path, AudioBuffer & output, std::string & error);
bool probe_audio(const std::string & path, earshot_audio_info & output, std::string & error);

}  // namespace earshot::detail

#endif
