#!/usr/bin/env bash
# Task 767: builds a Linux release archive from an existing build tree.
#
# The Linux counterpart of package_release.ps1, kept in a script for the same
# reason: the workflow and a local run produce the same archive. It builds
# nothing -- run scripts/build_linux_<arch>.sh --config Release
# --static-runtime first.
#
# tar.gz rather than zip because it keeps the executable bits.
set -euo pipefail

usage()
{
    cat <<'USAGE'
usage: package_release_linux.sh i386|x64 [--build-dir PATH] [--version X.Y.Z]
                                [--output-dir PATH]
Packages build/linux_<arch>_release (or --build-dir) into
<output-dir>/rePIU-v<version>-linux-<arch>.tar.gz. The version defaults to the
VERSION file and the output directory to build/package.
USAGE
}

if [[ $# -lt 1 ]]; then
    usage >&2
    exit 2
fi
arch="$1"
shift
case "$arch" in
    i386|x64) ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown architecture: $arch" >&2; usage >&2; exit 2 ;;
esac

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root/build/linux_${arch}_release"
version=""
output_dir="$root/build/package"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) build_dir="${2:?--build-dir needs a value}"; shift 2 ;;
        --version) version="${2:?--version needs a value}"; shift 2 ;;
        --output-dir) output_dir="${2:?--output-dir needs a value}"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z "$version" ]]; then
    version="$(tr -d '[:space:]' < "$root/VERSION")"
fi
version="${version#v}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "VERSION must use major.minor.patch format, found '$version'." >&2
    exit 1
fi

# SDL3 is linked statically and loads the desktop and audio libraries at run
# time, so nothing beyond these files travels with the archive.
binaries=(
    repiu
    repiu_launcher
    repiu_chd_cd_probe
    repiu_glide_issue_probe
    repiu_core_probe
)
documents=(VERSION README.md THIRD_PARTY_NOTICES.md LICENSE CREDITS.md)

missing=()
for binary in "${binaries[@]}"; do
    [[ -x "$build_dir/$binary" ]] || missing+=("$binary")
done
if [[ ${#missing[@]} -gt 0 ]]; then
    echo "Missing binaries in $build_dir: ${missing[*]}" >&2
    echo "Run scripts/build_linux_${arch}.sh --config Release --static-runtime" \
        "--build-dir $build_dir first." >&2
    exit 1
fi

name="rePIU-v${version}-linux-${arch}"
staging_root="$output_dir/staging-linux-${arch}"
staging="$staging_root/$name"
rm -rf "$staging_root"
mkdir -p "$staging"

for binary in "${binaries[@]}"; do
    cp "$build_dir/$binary" "$staging/"
done
# Release binaries keep their symbols otherwise; the archive does not need them.
strip --strip-unneeded "${binaries[@]/#/$staging/}"
for document in "${documents[@]}"; do
    cp "$root/$document" "$staging/"
done

archive="$output_dir/$name.tar.gz"
rm -f "$archive"
tar -C "$staging_root" --owner=0 --group=0 --numeric-owner -czf "$archive" "$name"
rm -rf "$staging_root"

echo "Packaging rePIU v$version for linux-$arch"
echo "  $(basename "$archive") ($(stat -c %s "$archive") bytes)"
