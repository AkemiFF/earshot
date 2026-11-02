#!/bin/sh
# Smoke-test the finished CPU AppImage without FUSE or model downloads.
set -eu

fail() { echo "AppImage validation failed: $*" >&2; exit 1; }

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

run() { "$image" --appimage-extract-and-run "$@"; }
version_output=$(run --version) || fail '--version failed'
printf '%s\n' "$version_output" | grep -Fq "$version" || fail "wrong version: $version_output"
help_output=$(run --help) || fail '--help failed'
printf '%s\n' "$help_output" | grep -Fq 'Usage: earshot' || fail '--help missing CLI usage'
models_output=$(run --list-models) || fail '--list-models failed'
printf '%s\n' "$models_output" | grep -Fq 'Models in ' || fail '--list-models missing catalogue'

echo "Validated CPU earshot $version AppImage"
