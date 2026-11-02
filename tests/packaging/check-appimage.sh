#!/bin/sh
# Smoke-test the finished CPU AppImage without FUSE or model downloads.
set -eu

fail() { echo "AppImage validation failed: $*" >&2; exit 1; }
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
check_glibc=$root/packaging/appimage/check-glibc.sh

# Exercise real ELF version-need records, including a dependency placed in
# usr/lib. /usr/bin/true on Ubuntu 22.04 needs GLIBC_2.34.
if [ "${1:-}" = --test-glibc ]; then
    [ "$#" -eq 1 ] || fail 'Usage: check-appimage.sh --test-glibc'
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    mkdir -p "$tmp/AppDir/usr/bin" "$tmp/AppDir/usr/lib"
    /bin/cp /usr/bin/true "$tmp/AppDir/usr/bin/earshot"
    readelf --version-info "$tmp/AppDir/usr/bin/earshot" | grep -q 'GLIBC_2\.34' ||
        fail 'fixture requires /usr/bin/true with GLIBC_2.34'
    sh "$check_glibc" "$tmp/AppDir" || fail 'rejected compatible CLI fixture'
    python3 - "$tmp/AppDir/usr/bin/earshot" <<'PY'
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
data = path.read_bytes()
assert b'GLIBC_2.34' in data
path.write_bytes(data.replace(b'GLIBC_2.34', b'GLIBC_2.35'))
PY
    sh "$check_glibc" "$tmp/AppDir" || fail 'rejected GLIBC_2.35 boundary fixture'
    python3 - "$tmp/AppDir/usr/bin/earshot" <<'PY'
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
path.write_bytes(path.read_bytes().replace(b'GLIBC_2.35', b'GLIBC_2.36'))
PY
    if sh "$check_glibc" "$tmp/AppDir" > "$tmp/error" 2>&1; then
        fail 'accepted CLI requiring GLIBC_2.36'
    fi
    grep -q 'GLIBC_2\.36' "$tmp/error" || fail 'CLI rejected for wrong reason'
    /bin/cp /usr/bin/true "$tmp/AppDir/usr/bin/earshot"
    /bin/cp "$tmp/AppDir/usr/bin/earshot" "$tmp/AppDir/usr/lib/libfixture.so"
    python3 - "$tmp/AppDir/usr/lib/libfixture.so" <<'PY'
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
path.write_bytes(path.read_bytes().replace(b'GLIBC_2.34', b'GLIBC_2.36'))
PY
    if sh "$check_glibc" "$tmp/AppDir" > "$tmp/error" 2>&1; then
        fail 'accepted bundled library requiring GLIBC_2.36'
    fi
    grep -q 'GLIBC_2\.36' "$tmp/error" || fail 'library rejected for wrong reason'
    echo 'Validated GLIBC baseline fixtures (CLI and bundled library)'
    exit 0
fi

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 earshot-VERSION-x86_64.AppImage [version]" >&2
    exit 2
fi

image=$1
version=${2:-$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' "$(dirname "$0")/../../CMakeLists.txt" | head -n 1)}
[ -n "$version" ] || fail 'cannot determine project version'
[ -f "$image" ] && [ -x "$image" ] || fail "missing executable AppImage: $image"
[ "$(basename "$image")" = "earshot-${version}-x86_64.AppImage" ] ||
    fail "unexpected AppImage filename: $image"

image=$(CDPATH= cd -- "$(dirname "$image")" && pwd)/$(basename "$image")
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
(cd "$tmp" && "$image" --appimage-extract > /dev/null) || fail 'could not extract AppImage'
sh "$check_glibc" "$tmp/squashfs-root" || fail 'AppImage exceeds GLIBC_2.35 baseline'

run() { "$image" --appimage-extract-and-run "$@"; }
version_output=$(run --version) || fail '--version failed'
printf '%s\n' "$version_output" | grep -Fq "$version" || fail "wrong version: $version_output"
help_output=$(run --help) || fail '--help failed'
printf '%s\n' "$help_output" | grep -Fq 'Usage: earshot' || fail '--help missing CLI usage'
models_output=$(run --list-models) || fail '--list-models failed'
printf '%s\n' "$models_output" | grep -Fq 'Models in ' || fail '--list-models missing catalogue'

echo "Validated CPU earshot $version AppImage"
