#!/bin/sh
# Exercise the same DT_NEEDED policy used for published CUDA packages.
set -eu
script=$(CDPATH= cd -- "$(dirname "$0")" && pwd)/check-debs.sh
fixture=$(mktemp)
trap 'rm -f "$fixture"' EXIT HUP INT TERM

cat > "$fixture" <<'EOF'
 0x0000000000000001 (NEEDED)             Shared library: [libcudart.so.12]
 0x0000000000000001 (NEEDED)             Shared library: [libcublas.so.12]
 0x0000000000000001 (NEEDED)             Shared library: [libcublasLt.so.12]
 0x0000000000000001 (NEEDED)             Shared library: [libcuda.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libgomp.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libstdc++.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libgcc_s.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [ld-linux-x86-64.so.2]
EOF
sh "$script" --test-cuda-needed "$fixture" || exit 1
printf '%s\n' ' 0x0000000000000001 (NEEDED)             Shared library: [libnccl.so.2]' >> "$fixture"
if sh "$script" --test-cuda-needed "$fixture"; then
    echo 'Expected unexpected NCCL dependency to be rejected' >&2
    exit 1
fi
sed '/\[libnccl.so.2\]/d; s/\[libcudart.so.12\]/[libcudart.so.13]/' "$fixture" > "$fixture.next"
mv "$fixture.next" "$fixture"
if sh "$script" --test-cuda-needed "$fixture"; then
    echo 'Expected unsupported CUDA SONAME major to be rejected' >&2
    exit 1
fi
echo 'Validated CUDA DT_NEEDED allowlist and unexpected-library rejection'
