#!/bin/sh
# Validate both release variants without installing either package.
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 CPU.deb CUDA.deb [version]" >&2
    exit 2
fi

cpu=$1
cuda=$2
version=${3:-$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' "$(dirname "$0")/../../CMakeLists.txt" | head -n 1)}
[ -n "$version" ] || { echo 'Cannot determine project version' >&2; exit 1; }

fail() { echo "Package validation failed: $*" >&2; exit 1; }
field() { dpkg-deb -f "$1" "$2"; }

check_package() {
    deb=$1
    name=$2
    other=$3
    [ -f "$deb" ] || fail "missing $deb"
    [ "$(field "$deb" Package)" = "$name" ] || fail "$deb: expected Package: $name"
    [ "$(field "$deb" Version)" = "$version" ] || fail "$deb: expected Version: $version"
    arch=$(field "$deb" Architecture)
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
printf '%s\n' "$cuda_depends" | grep -Eq '(^|, *)libcuda1([ (]|,|$)' ||
    fail "$cuda: Depends must include a CUDA driver alternative (libcuda1)"

echo "Validated earshot and earshot-cuda $version packages"
