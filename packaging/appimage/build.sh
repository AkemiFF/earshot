#!/bin/sh
# Build the CPU-only x86_64 AppImage from an already configured and built tree.
# Configure externally on Ubuntu 22.04 with -DEARSHOT_CUDA=OFF -DGGML_NATIVE=OFF.
set -eu

fail() { echo "AppImage build failed: $*" >&2; exit 1; }
[ "$#" -eq 2 ] || fail 'Usage: build.sh CPU_BUILD_DIR OUTPUT_DIR'

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(CDPATH= cd -- "$1" && pwd)
mkdir -p "$2"
output_dir=$(CDPATH= cd -- "$2" && pwd)
[ "$(uname -m)" = x86_64 ] || fail 'requires an x86_64 build host'
[ -f "$build/CMakeCache.txt" ] || fail 'missing configured CMake build'
grep -qx 'EARSHOT_CUDA:STRING=OFF' "$build/CMakeCache.txt" || fail 'EARSHOT_CUDA must be OFF'
grep -qx 'GGML_CUDA:BOOL=OFF' "$build/CMakeCache.txt" || fail 'GGML_CUDA must be OFF'
grep -qx 'GGML_NATIVE:BOOL=OFF' "$build/CMakeCache.txt" || fail 'GGML_NATIVE must be OFF'
version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt" | head -n 1)
[ -n "$version" ] || fail 'missing CMake project version'

work=$(mktemp -d "$output_dir/.earshot-appimage.XXXXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
appdir=$work/AppDir
mkdir -p "$appdir/usr/bin" "$appdir/usr/lib" "$appdir/usr/share/licenses/earshot"

# Install to a disposable prefix, then copy only the CLI (not headers, archives,
# CMake metadata, or model weights) into the actual AppDir.
cmake --install "$build" --prefix "$work/install/usr" > "$work/install.log"
[ -x "$work/install/usr/bin/earshot" ] || fail 'CMake install did not produce the CLI'
/bin/cp "$work/install/usr/bin/earshot" "$appdir/usr/bin/earshot"
/bin/cp "$root/packaging/appimage/AppRun" "$root/packaging/appimage/earshot.desktop" \
    "$root/packaging/appimage/earshot.svg" "$appdir/"
/bin/chmod +x "$appdir/AppRun"
/bin/cp "$root/LICENSE" "$appdir/usr/share/licenses/earshot/LICENSE"
if [ -f "$build/_deps/whisper_cpp-src/LICENSE" ]; then
    /bin/cp "$build/_deps/whisper_cpp-src/LICENSE" "$appdir/usr/share/licenses/earshot/whisper.cpp-LICENSE"
fi
# miniaudio's choice of public-domain / MIT-0 text is at the end of its header.
sed -n '/^This software is available as a choice of the following licenses\./,$p' \
    "$root/third_party/miniaudio.h" > "$appdir/usr/share/licenses/earshot/miniaudio-LICENSE"

# Bundle the C++ and OpenMP runtimes, but rely on the Ubuntu 22.04+ base for
# glibc/loader/libgcc. ldd catches any unexpected dynamic dependencies early.
deps=$(env -u LD_PRELOAD ldd "$appdir/usr/bin/earshot") || fail 'ldd failed on CLI'
printf '%s\n' "$deps" | grep -q 'not found' && fail 'CLI has missing dependencies'
if printf '%s\n' "$deps" | grep -Eiq 'cuda|cublas|cudart'; then
    fail 'CPU CLI unexpectedly links CUDA libraries'
fi
libs=$(printf '%s\n' "$deps" | awk '$2 == "=>" && $3 ~ /^\// {print $1 " " $3}')
while read -r soname path; do
    [ -n "$soname" ] || continue
    case "$soname" in
        libstdc++.so.6) package=libstdc++6 ;;
        libgomp.so.1) package=libgomp1 ;;
        libc.so.6|libm.so.6|libgcc_s.so.1|libpthread.so.0|libdl.so.2|librt.so.1|libresolv.so.2|libutil.so.1|*/ld-linux-x86-64.so.2) continue ;;
        *) fail "unreviewed runtime dependency: $soname" ;;
    esac
    /bin/cp -L "$path" "$appdir/usr/lib/$soname"
    if [ -f "/usr/share/doc/$package/copyright" ]; then
        /bin/cp "/usr/share/doc/$package/copyright" \
            "$appdir/usr/share/licenses/earshot/$package-copyright"
    else
        # Arch and some other distros keep the GCC runtime exception under
        # /usr/share/licenses rather than Debian's package copyright path.
        case "$package" in
            libgomp1) license_dir=/usr/share/licenses/libgomp ;;
            libstdc++6) license_dir=/usr/share/licenses/libstdc++ ;;
        esac
        [ -f "$license_dir/RUNTIME.LIBRARY.EXCEPTION" ] &&
            [ -f /usr/share/licenses/man-pages/GPL-3.0-or-later.txt ] ||
            fail "missing $package license"
        /bin/cp "$license_dir/RUNTIME.LIBRARY.EXCEPTION" \
            "$appdir/usr/share/licenses/earshot/$package-runtime-exception"
        /bin/cp /usr/share/licenses/man-pages/GPL-3.0-or-later.txt \
            "$appdir/usr/share/licenses/earshot/$package-GPL-3.0"
    fi
done <<EOF
$libs
EOF

# Pin both the release URL and the exact digest; never execute an unchecked
# downloaded binary. Run via extraction so CI does not need FUSE.
tool=$work/appimagetool.AppImage
curl -fL --retry 3 --max-time 120 -o "$tool" \
    'https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage'
printf '%s  %s\n' \
    'ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0' "$tool" |
    sha256sum -c - || fail 'appimagetool checksum mismatch'
/bin/chmod +x "$tool"
runtime=$work/runtime-x86_64
curl -fL --retry 3 --max-time 120 -o "$runtime" \
    'https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64'
printf '%s  %s\n' \
    '2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d' "$runtime" |
    sha256sum -c - || fail 'AppImage runtime checksum mismatch'
image="$output_dir/earshot-${version}-x86_64.AppImage"
staged_image="$work/earshot-${version}-x86_64.AppImage"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --runtime-file "$runtime" \
    --mksquashfs-opt -processors --mksquashfs-opt 2 "$appdir" "$staged_image"
/bin/chmod +x "$staged_image"
mv "$staged_image" "$image"
echo "Built $image"
