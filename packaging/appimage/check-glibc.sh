#!/bin/sh
# Inspect every ELF in a staged/extracted AppDir, not the host's linked paths.
# Ubuntu 22.04 provides glibc 2.35; definitions are not requirements, so only
# inspect the ELF version-needs section (.gnu.version_r).
set -eu

fail() { echo "GLIBC baseline check failed: $*" >&2; exit 1; }
[ "$#" -eq 1 ] && [ -d "$1/usr" ] || fail 'Usage: check-glibc.sh AppDir'
command -v readelf > /dev/null || fail 'readelf is required (binutils)'
LC_ALL=C readelf -h "$1/usr/bin/earshot" > /dev/null 2>&1 ||
    fail 'missing ELF CLI at usr/bin/earshot'

find "$1/usr" -type f | while IFS= read -r elf; do
    LC_ALL=C readelf -h "$elf" > /dev/null 2>&1 || continue
    versions=$(LC_ALL=C readelf --version-info "$elf") || fail "cannot read ELF versions in $elf"
    printf '%s\n' "$versions" | awk -v elf="$elf" '
        /^Version needs section / { needs = 1; next }
        /^Version (symbols|definition) section / { needs = 0 }
        needs && /Name: GLIBC_PRIVATE/ {
            printf "%s requires GLIBC_PRIVATE (unstable ABI)\n", elf > "/dev/stderr"
            bad = 1
        }
        needs && /Name: GLIBC_[0-9]+\.[0-9]+/ {
            version = $0
            sub(/^.*Name: GLIBC_/, "", version)
            sub(/[^0-9.].*$/, "", version)
            split(version, parts, /\./)
            if (parts[1] + 0 > 2 || (parts[1] + 0 == 2 && parts[2] + 0 > 35)) {
                printf "%s requires GLIBC_%s (maximum GLIBC_2.35)\n", elf, version > "/dev/stderr"
                bad = 1
            }
        }
        END { exit bad }
    ' || fail "unsupported GLIBC requirement in $elf"
done
