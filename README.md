# earshot

[![CI](https://github.com/AkemiFF/earshot/actions/workflows/ci.yml/badge.svg)](https://github.com/AkemiFF/earshot/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/AkemiFF/earshot)](https://github.com/AkemiFF/earshot/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

`earshot` is a local speech-to-text library and command-line tool built on
[whisper.cpp](https://github.com/ggml-org/whisper.cpp). It runs offline after
the model has been downloaded and supports CPU and CUDA acceleration.

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DEARSHOT_CUDA=ON
cmake --build build -j$(nproc)
./build/earshot --list-models
./build/earshot -l en --format txt,srt lesson.mp3
```

The default model is `large-v3-turbo`. Models are cached in
`$XDG_CACHE_HOME/earshot/models` or `~/.cache/earshot/models` and are downloaded
only when needed. Use `--no-vad` to avoid downloading the optional Silero VAD
model.

```bash
earshot --help
earshot --device cpu -m small.en interview.m4a
earshot --translate -l fr --format vtt talk.wav
earshot -p --stdout podcast.mp3
```

Prebuilt Linux packages are published on the
[GitHub Releases page](https://github.com/AkemiFF/earshot/releases) when
available. The Debian package is CPU-only and works on Debian, Ubuntu and
Linux Mint. Arch users can build the PKGBUILD in `packaging/aur` while the
AUR package is unavailable.

Supported output formats are plain text, SRT, WebVTT and JSON. Input decoding
is provided by vendored miniaudio, so ffmpeg is not required at runtime.

## Library use

The stable C API is in `include/earshot/earshot.h`; a RAII C++17 wrapper is in
`include/earshot/earshot.hpp`:

```cpp
#include <earshot/earshot.hpp>

earshot::Options options;
options.model_path = "/models/ggml-large-v3-turbo.bin";
options.language = "en";
earshot::Transcriber transcriber(options);
const earshot::Result result = transcriber.transcribe("meeting.mp3");
std::cout << result.text() << '\n';
```

## Music and vocals

Whisper can transcribe clear vocals, speech over quiet music, and some songs.
Loud instrumentation, concerts, choirs and overlapping voices reduce accuracy.
The core intentionally does not bundle a source-separation model. A future
optional `earshot-music` component can run Demucs or UVR before transcription;
that keeps the normal installation small and avoids imposing another model and
its licence on every user.

## Build options

| Option | Default | Meaning |
|---|---:|---|
| `EARSHOT_BUILD_CLI` | ON | Build the `earshot` command |
| `EARSHOT_BUILD_TESTS` | ON | Build the offline unit tests |
| `EARSHOT_CUDA` | ON | Detect and use the CUDA backend when available |
| `EARSHOT_USE_SYSTEM_WHISPER` | OFF | Use a system whisper.cpp package |
| `EARSHOT_WHISPER_GIT_TAG` | `v1.9.5` | Pinned whisper.cpp source version |

CPU-only build:

```bash
cmake -B build -DEARSHOT_CUDA=OFF
```

Run tests with:

```bash
cmake -B build-tests -DEARSHOT_CUDA=OFF -DEARSHOT_BUILD_TESTS=ON
cmake --build build-tests -j$(nproc)
ctest --test-dir build-tests --output-on-failure
```

## Packaging roadmap

The project is intentionally split into a small executable/library and
separately managed model weights. The first packaging targets should be:

1. Arch AUR (`earshot` and `earshot-bin`);
2. Debian/Ubuntu `.deb` and an APT repository;
3. AppImage for portable CLI use;
4. Flatpak for a graphical frontend.

CUDA, VAD and vocal separation should remain optional package features. Model
weights must not be silently embedded in the base package.

## Development

```bash
git clone https://github.com/AkemiFF/earshot.git
cd earshot
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DEARSHOT_CUDA=OFF \
  -DEARSHOT_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md)
before opening a pull request.

## Licence

The earshot code is MIT licensed. whisper.cpp and miniaudio are included under
their respective licences. Whisper and VAD weights are downloaded separately
and retain the licence terms published by their model authors.
