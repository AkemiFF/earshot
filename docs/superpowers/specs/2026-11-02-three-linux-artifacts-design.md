# Three Linux release artifacts — design

## Goal

Release earshot v0.1.3 with three distinct x86-64 artifacts: a CPU Debian
package, a CUDA Debian package, and a CPU AppImage. Model weights stay separate.

## Compatibility and distribution

Build in pinned Ubuntu 22.04 environments for a glibc 2.35 baseline, targeting
Ubuntu 22.04+, Debian 12+ and compatible derivatives. Set `GGML_NATIVE=OFF`
for portable CPU binaries. The AppImage bundles non-base runtime libraries but
never glibc, and its final executable is smoke-tested without FUSE.

The CUDA package is an alternative to the CPU package, not an add-on: both
install the same CLI and development files. Its Debian package name is
`earshot-cuda`; reciprocal Conflicts/Replaces permit switching variants. Build
with an explicitly pinned CUDA toolkit, explicitly targeted NVIDIA
architectures including sm_86, and one or two jobs to limit peak memory.
CUDA runtime libraries and a compatible NVIDIA driver must be installed by the
user; neither drivers nor models are bundled. Document the toolkit/runtime
version and inspect `Depends` in generated package metadata. No GPU runner is
available in GitHub Actions, so GPU inference must be tested locally before
calling the CUDA package runtime-verified.

## Build pipeline

Independent CPU Debian, CUDA Debian, and CPU AppImage jobs use distinct build
directories. The Debian jobs use CPack DEB and verify package name, version,
architecture, conflict/replacement relationships, dependencies and contents.
The AppImage job stages only CLI and required shared libraries in an AppDir,
adds a terminal desktop entry and icon, creates an x86_64 AppImage using pinned
tools, and tests the finished image's `--version`/`--help`/`--list-models`.
The release job publishes all three artifacts only after the build jobs pass;
failed or partial builds must not be labeled complete.

## Documentation and verification

README describes installation and runtime prerequisites for all three variants
and accurately identifies the CPU-only AppImage. The version string is updated
consistently across CMake, API, CLI, JSON, tests and packaging. Before tagging,
run CPU CTest, packaging metadata checks, AppImage smoke tests and an available
RTX 3070 transcription test; bound concurrent CUDA builds and do not run them
alongside other memory-heavy builds. AUR metadata and SHA256 are finalized from
the published immutable tag archive, in a separate post-tag commit.
