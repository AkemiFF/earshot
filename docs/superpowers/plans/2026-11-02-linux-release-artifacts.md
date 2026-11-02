# Three Linux Release Artifacts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish v0.1.3 with CPU Debian, CUDA Debian, and CPU AppImage artifacts.

**Architecture:** Separate build jobs on Ubuntu 22.04 generate independent packages and upload them as artifacts. A gated publish job uploads all three files to one GitHub release only if each job succeeds. Model weights are never embedded.

**Tech Stack:** CMake, CPack DEB, CUDA toolkit, GitHub Actions, AppDir/AppImage tooling, shell validation, dpkg-deb.

---

### Task 1: Portable package configuration

**Files:** Modify `CMakeLists.txt`; create `tests/packaging/check-debs.sh`.

- [ ] **Step 1: Add packaging test contract.** Write a shell script that invokes `dpkg-deb -f "$cpu" Package Version Architecture Conflicts Replaces` and likewise for CUDA; assert CPU package `earshot`, CUDA package `earshot-cuda`, amd64, version 0.1.3, reciprocal conflicts and replaces. Exit nonzero if assertions fail.
- [ ] **Step 2: Demonstrate failure.** Generate existing package and run script; expect CUDA package/configuration missing.
- [ ] **Step 3: Implement CPack options.** Set `CPACK_DEBIAN_FILE_NAME=DEB-DEFAULT`, `CPACK_DEBIAN_PACKAGE_NAME` conditional on `EARSHOT_CUDA`, reciprocal Conflicts/Replaces, SHLIBDEPS ON; disable native ggml CPU tuning for public artifacts via CI flags, and ensure CUDA ON fails loudly when toolkit absent.
- [ ] **Step 4: Verify CPU.** Configure CPU release under `/tmp/opencode/earshot-cpu-build` with `-DEARSHOT_CUDA=OFF -DGGML_NATIVE=OFF`; build `--parallel 2`, run cpack, inspect `dpkg-deb -f` on Debian-capable host/container.
- [ ] **Step 5: Commit.** `git add CMakeLists.txt tests/packaging/check-debs.sh && git commit -m 'packaging: distinguish CPU and CUDA Debian variants'`.

### Task 2: CPU AppImage packaging

**Files:** Create `packaging/appimage/AppRun`, `packaging/appimage/earshot.desktop`, `packaging/appimage/earshot.svg`, `packaging/appimage/build.sh`, `tests/packaging/check-appimage.sh`.

- [ ] **Step 1: Define tests.** `check-appimage.sh` checks executable file, runs `--appimage-extract-and-run --version`, `--help`, and `--list-models`, and checks output/version without downloading a model.
- [ ] **Step 2: Demonstrate failure.** Run checker on nonexistent image and verify nonzero exit.
- [ ] **Step 3: Implement staging.** Install CPU build into AppDir, add desktop/icon and AppRun, bundle only non-base dependencies, build with checksum-pinned AppImage tools, include library licenses, name file `earshot-0.1.3-x86_64.AppImage`.
- [ ] **Step 4: Verify on pinned Ubuntu 22.04 container/runner.** Test final AppImage (not only AppDir) via script, inspect `readelf` GLIBC requirements.
- [ ] **Step 5: Commit.** `git add packaging/appimage tests/packaging/check-appimage.sh && git commit -m 'packaging: build portable CPU AppImage'`.

### Task 3: Release CI integration

**Files:** Modify `.github/workflows/release.yml`.

- [ ] **Step 1: Validate workflow contract.** Parse YAML and assert three independent artifact jobs on `ubuntu-22.04`, release `needs` all three, CUDA build architecture includes sm_86 and limits parallelism.
- [ ] **Step 2: Demonstrate failure.** Current single-job release does not pass assertions.
- [ ] **Step 3: Implement jobs.** Pin CUDA installation, build `earshot-cuda`, verify GGML_CUDA ON and inspect dpkg dependency metadata; upload CPU/CUDA DEBs and CPU AppImage as job artifacts. Publish only after download of all three and checks. Do not embed Whisper models.
- [ ] **Step 4: Verify config.** Run YAML validator/actionlint if available, inspect each generated artifact on GitHub Actions; no local concurrent CUDA builds.
- [ ] **Step 5: Commit.** `git add .github/workflows/release.yml && git commit -m 'ci: publish three validated Linux packages'`.

### Task 4: Documentation, version and release

**Files:** Modify `README.md`, `CMakeLists.txt`, `cli/version.hpp`, `src/earshot.cpp`, `src/formats.cpp`, `include/earshot/earshot.h`, `tests/test_api.cpp`, `packaging/aur/PKGBUILD`, `packaging/debian/changelog`.

- [ ] **Step 1: Assert version consistency.** Search tracked files for `0.1.2` and assert release versions equal `0.1.3` after edits.
- [ ] **Step 2: Edit version and docs.** Explain CPU/CUDA package replacement, CUDA driver/toolkit runtime requirement, Ubuntu 22.04+/Debian 12+ baseline, AppImage CPU-only and separate model download.
- [ ] **Step 3: Verify.** CPU CTest; AppImage smoke test; Debian metadata tests; real CUDA transcription with RTX 3070 after CUDA package exists; `git diff --check`.
- [ ] **Step 4: Commit and release.** Push commits only after local validation; create annotated `v0.1.3` tag, push; verify all three artifacts in GitHub release and install/runtime smoke test before advertising.
- [ ] **Step 5: Finish AUR checksum.** Download immutable `v0.1.3` tarball, run `sha256sum`, update `packaging/aur/PKGBUILD`, regenerate ignored `.SRCINFO`, `makepkg --verifysource -f`, commit/push checksum; do not submit AUR while registrations are closed.
