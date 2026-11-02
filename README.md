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
cmake --build build --parallel 2
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

When a release is published, its [GitHub Releases page](https://github.com/AkemiFF/earshot/releases)
provides three x86-64 artifacts:

| Artifact | Variant | Install/run |
|---|---|---|
| `earshot_0.1.3_amd64.deb` | CPU-only Debian package | `sudo apt install ./earshot_0.1.3_amd64.deb` |
| `earshot-cuda_0.1.3_amd64.deb` | CUDA Debian package | `sudo apt install ./earshot-cuda_0.1.3_amd64.deb` |
| `earshot-0.1.3-x86_64.AppImage` | CPU-only portable CLI | `chmod +x earshot-0.1.3-x86_64.AppImage && ./earshot-0.1.3-x86_64.AppImage --help` |

The `.deb` packages are **alternatives**, not add-ons: both install the same
`earshot` command and development files, and installing one replaces the other.
The CUDA package is built with CUDA toolkit **12.6** and needs the CUDA 12.6
runtime libraries (`cuda-cudart-12-6` and `libcublas-12-6`) and a compatible
NVIDIA driver on the target machine. It does not bundle the driver; install the
required runtime packages from NVIDIA's Ubuntu 22.04 CUDA repository if they
are not already available. Install a CUDA 12.6-compatible NVIDIA driver for
your GPU separately (for example via Ubuntu's driver manager) **before** using
the CUDA variant; the driver is not a hard `.deb` dependency because driver
package names vary by distribution and `libcuda1` is not available in Ubuntu
22.04's repositories. The AppImage does **not** provide GPU acceleration.

On Ubuntu 22.04, configure the NVIDIA CUDA repository before installing the
CUDA package (follow NVIDIA's [official installation guide](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html)
for other distributions):

```bash
curl -fLO https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update
sudo apt install ./earshot-cuda_0.1.3_amd64.deb
```

The CPU packages use a conservative x86-64 instruction baseline for portability;
building locally with native CPU optimizations may improve CPU-only speed.

These x86-64 artifacts are built on Ubuntu 22.04 with an Ubuntu 22.04+ /
Debian 12+ baseline; compatibility with other distributions or older releases
is not guaranteed. Model weights are not bundled with any artifact and are
downloaded separately when needed. Arch users can build the PKGBUILD in
`packaging/aur` while the AUR package is unavailable. Verify its checksum
against the published tag archive before installing a new release.

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

## Packaging notes

The executable and library are packaged separately from model weights. The
release pipeline targets CPU and CUDA Debian packages and a CPU AppImage;
published assets should be checked on the release page before installing.
Optional VAD weights are downloaded separately; vocal separation is not
included in these packages.

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
