# Contributing to earshot

Thanks for helping improve earshot.

## Before opening an issue

Please search existing issues first. For transcription quality problems,
include the model name, language, output format and whether CPU or CUDA was
used. Do not upload private audio or model files.

## Building and testing

The CPU-only build is the baseline used by CI:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DEARSHOT_CUDA=OFF \
  -DEARSHOT_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

CUDA builds are optional. Keep model weights and audio files outside the
repository; they must never be committed.

## Pull requests

- Explain the user-facing effect of the change.
- Add or update tests when behavior changes.
- Keep commits focused and documentation in English.
- Do not add bundled Whisper or VAD model weights.

## Security and privacy

earshot is designed for local processing, but reports should not contain
private recordings or transcripts. For security issues, contact the
maintainer privately instead of opening a public issue.
