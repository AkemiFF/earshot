# Vendored third party code

`miniaudio.h` — single header audio decoder by David Reid (mackron), public domain
or MIT-0. It provides MP3, WAV, FLAC and Ogg/Vorbis decoding through dr_mp3,
dr_wav, dr_flac and stb_vorbis, so earshot does not depend on ffmpeg at runtime.

Update with:

    curl -L -o third_party/miniaudio.h \
      https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
