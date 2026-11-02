#!/bin/sh
# Validate both release variants without installing either package.
set -eu

fail() { echo "Package validation failed: $*" >&2; exit 1; }

# With shlibdeps disabled for CUDA, only explicitly mapped libraries may be
# linked into the CLI. Use SONAMEs, not prefixes, so new majors fail closed.
check_cuda_needed() {
    needed=$(sed -n '/(NEEDED)/s/.*\[\([^]]*\)\].*/\1/p' "$1")
    [ -n "$needed" ] || fail "no ELF DT_NEEDED entries in $1"
    for library in libcudart.so.12 libcublas.so.12 libcuda.so.1; do
        printf '%s\n' "$needed" | grep -Fxq "$library" ||
            fail "$1: expected CUDA shared library $library"
    done
    while IFS= read -r library; do
        case "$library" in
            libcudart.so.12|libcublas.so.12|libcublasLt.so.12|libcuda.so.1|\
            libgomp.so.1|libstdc++.so.6|libgcc_s.so.1|\
            libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|ld-linux-x86-64.so.2) ;;
            *) fail "$1: unexpected ELF dependency $library (add an explicit Debian dependency or remove the linkage)" ;;
        esac
    done <<EOF
$needed
EOF
}

if [ "${1:-}" = --test-cuda-needed ]; then
    [ "$#" -eq 2 ] || fail 'Usage: check-debs.sh --test-cuda-needed READELF-DYNAMIC-OUTPUT'
    check_cuda_needed "$2"
    exit 0
fi

# Check the generated CPack configuration, not just the resulting metadata:
# shlibdeps can fail before a CUDA .deb exists on a driverless build runner.
if [ "${1:-}" = --test-cpack-metadata ]; then
    [ "$#" -eq 3 ] || fail 'Usage: check-debs.sh --test-cpack-metadata CPU-CPackConfig CUDA-CPackConfig'
    grep -Eq '^set\(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "ON"\)$' "$2" ||
        fail 'CPU package must retain shlibdeps scanning'
    grep -Eq '^set\(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "OFF"\)$' "$3" ||
        fail 'CUDA package must skip shlibdeps scanning on driverless runners'
    grep -Eq '^set\(CPACK_DEBIAN_PACKAGE_DEPENDS ".*cuda-cudart-12-6, libcublas-12-6' "$3" ||
        fail 'CUDA 12.6 runtime dependencies missing from CPack configuration'
    echo 'Validated CPU and CUDA CPack metadata'
    exit 0
fi

# Exercise the cache transitions independently of CUDA compilation. Supply a
# local whisper.cpp checkout to avoid fetching dependencies during the test.
if [ "${1:-}" = --test-cuda-config ]; then
    [ "$#" -eq 2 ] || fail 'Usage: check-debs.sh --test-cuda-config WHISPER_SOURCE'
    source_dir=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
    whisper_dir=$(CDPATH= cd -- "$2" && pwd)
    build_dir=$(mktemp -d)
    trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
    configure() {
        cmake -S "$source_dir" -B "$build_dir" -DEARSHOT_BUILD_TESTS=OFF \
            -DFETCHCONTENT_SOURCE_DIR_WHISPER_CPP="$whisper_dir" \
            -DEARSHOT_NVCC:FILEPATH=OFF "$@" > "$build_dir/configure.log" 2>&1
    }
    configure || fail 'automatic CPU configuration failed'
    grep -q '^GGML_CUDA:BOOL=OFF$' "$build_dir/CMakeCache.txt" || fail 'AUTO did not select CPU'
    if configure -DEARSHOT_CUDA=ON; then
        fail 'explicit CUDA request silently fell back to CPU after AUTO'
    fi
    grep -q 'EARSHOT_CUDA=ON was requested' "$build_dir/configure.log" ||
        fail 'explicit CUDA failed for the wrong reason'
    configure -DEARSHOT_CUDA=OFF -DGGML_CUDA=ON || fail 'explicit CPU configuration failed'
    grep -q '^GGML_CUDA:BOOL=OFF$' "$build_dir/CMakeCache.txt" ||
        fail 'OFF retained a stale CUDA backend'
    configure -DEARSHOT_CUDA=AUTO -DGGML_CUDA=ON || fail 'AUTO reconfiguration failed'
    grep -q '^GGML_CUDA:BOOL=OFF$' "$build_dir/CMakeCache.txt" ||
        fail 'AUTO retained a stale CUDA backend'
    configure -DEARSHOT_CUDA=ON -DEARSHOT_CUDA_AUTO_DEFAULT=ON ||
        fail 'legacy automatic CUDA cache did not migrate to AUTO'
    grep -q '^EARSHOT_CUDA:STRING=AUTO$' "$build_dir/CMakeCache.txt" ||
        fail 'legacy automatic CUDA cache did not become AUTO'
    if configure -DEARSHOT_CUDA=ON; then
        fail 'explicit CUDA request after legacy migration silently fell back to CPU'
    fi
    grep -q 'EARSHOT_CUDA=ON was requested' "$build_dir/configure.log" ||
        fail 'explicit CUDA after legacy migration failed for the wrong reason'
    echo 'Validated CUDA configure/reconfigure transitions'
    exit 0
fi

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 CPU.deb CUDA.deb [version]" >&2
    exit 2
fi

cpu=$1
cuda=$2
version=${3:-$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' "$(dirname "$0")/../../CMakeLists.txt" | head -n 1)}
[ -n "$version" ] || { echo 'Cannot determine project version' >&2; exit 1; }

field() { dpkg-deb -f "$1" "$2"; }

check_package() {
    deb=$1
    name=$2
    other=$3
    [ -f "$deb" ] || fail "missing $deb"
    [ "$(field "$deb" Package)" = "$name" ] || fail "$deb: expected Package: $name"
    [ "$(field "$deb" Version)" = "$version" ] || fail "$deb: expected Version: $version"
    arch=$(field "$deb" Architecture)
    [ "$arch" = amd64 ] || fail "$deb: expected Architecture: amd64 (got $arch)"
    [ "$(basename "$deb")" = "${name}_${version}_${arch}.deb" ] ||
        fail "$deb: expected Debian-default filename ${name}_${version}_${arch}.deb"
    for relation in Conflicts Replaces; do
        value=$(field "$deb" "$relation")
        printf '%s\n' "$value" | grep -Eq "(^|, *)${other}([ (]|,|$)" ||
            fail "$deb: $relation must include $other"
    done
    depends=$(field "$deb" Depends)
    for dependency in libc6 libgcc-s1 'libstdc\+\+6'; do
        printf '%s\n' "$depends" | grep -Eq "(^|, *)${dependency}([ (]|,|$)" ||
            fail "$deb: Depends must include $dependency"
    done
    recommends=$(field "$deb" Recommends)
    printf '%s\n' "$recommends" | grep -Eq '(^|, *)curl *\| *wget([ (]|,|$)' ||
        fail "$deb: Recommends must include curl | wget for model downloads"
    dpkg-deb -c "$deb" | awk '{print $NF}' | grep -Eq '^\./usr/bin/earshot$' ||
        fail "$deb: missing CLI executable"
    dpkg-deb -c "$deb" | awk '{print $NF}' | grep -Eq '^\./usr/lib(/[^/]+)?/libearshot\.a$' ||
        fail "$deb: missing static library"
    dpkg-deb -c "$deb" | awk '{print $NF}' | grep -Eq '^\./usr/include/earshot/earshot\.h$' ||
        fail "$deb: missing public headers"
}

check_package "$cpu" earshot earshot-cuda
check_package "$cuda" earshot-cuda earshot

cuda_depends=$(field "$cuda" Depends)
for dependency in libgomp1 cuda-cudart-12-6 libcublas-12-6; do
    printf '%s\n' "$cuda_depends" | grep -Eq "(^|, *)${dependency}([ (]|,|$)" ||
        fail "$cuda: Depends must include $dependency"
done
printf '%s\n' "$cuda_depends" | grep -Eq '(^|, *)(libcuda1|nvidia-driver)([ (]|,|$)' &&
    fail "$cuda: Depends must not require an unavailable driver package; install a compatible driver separately"

command -v readelf > /dev/null 2>&1 || fail 'readelf (binutils) is required for CUDA dependency validation'
binary_dir=$(mktemp -d)
trap 'rm -rf "$binary_dir"' EXIT HUP INT TERM
dpkg-deb --fsys-tarfile "$cuda" | tar -xOf - ./usr/bin/earshot > "$binary_dir/earshot" ||
    fail "$cuda: cannot extract CLI for ELF dependency validation"
readelf -d "$binary_dir/earshot" > "$binary_dir/dynamic" ||
    fail "$cuda: CLI is not a readable ELF binary"
check_cuda_needed "$binary_dir/dynamic"

echo "Validated earshot and earshot-cuda $version packages"
