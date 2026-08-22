#!/usr/bin/env bash
# Task 501. Builds the platform-neutral core and its probe as 32-bit x86 on
# Linux.
#
# The architecture is not a preference. The guest's 32-bit x86 code runs
# natively in the host process, exactly as it does under the Win32 host, so the
# host itself has to be a 32-bit process.
#
# Only the targets that exist on Linux today are built. The execution engine,
# the loader, and the launcher are still Win32-only; bringing them over is
# Stage 3 and Stage 2 of the port.
set -euo pipefail

configuration="Debug"
targets=()

usage()
{
    cat <<'USAGE'
usage: build_linux_i386.sh [--config Debug|Release|RelWithDebInfo|MinSizeRel]
                           [--target NAME]...
Builds into build/linux_i386. With no --target, every default target is built.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            configuration="${2:?--config needs a value}"
            shift 2
            ;;
        --target)
            targets+=("${2:?--target needs a value}")
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            usage
            exit 2
            ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root/build/linux_i386"

# A missing 32-bit toolchain otherwise surfaces as hundreds of header errors
# with the actual cause buried, so it is checked up front and named.
missing=0
if ! echo 'int main(void){return 0;}' | cc -m32 -x c - -o /dev/null 2>/dev/null
then
    missing=1
fi
if ! echo 'int main(){return 0;}' | c++ -m32 -x c++ - -o /dev/null 2>/dev/null
then
    missing=1
fi
if [[ $missing -ne 0 ]]; then
    cat >&2 <<'NEEDS'
The 32-bit toolchain is not usable. On Debian or Ubuntu:

    sudo apt update && sudo apt install -y gcc-multilib g++-multilib libc6-dev-i386

NEEDS
    exit 1
fi

# SDL3 refuses to configure without X11 or Wayland development packages, which
# Stage 1 does not need because nothing here opens a window. This is SDL's own
# documented escape hatch; Stage 2, which brings the launcher over, installs the
# real desktop packages instead.
cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$configuration" \
    -DSDL_UNIX_CONSOLE_BUILD=ON \
    -DCMAKE_C_FLAGS=-m32 \
    -DCMAKE_CXX_FLAGS=-m32 \
    -DCMAKE_EXE_LINKER_FLAGS=-m32 \
    -DCMAKE_SHARED_LINKER_FLAGS=-m32

build_arguments=("--build" "$build_dir" "--parallel")
for name in "${targets[@]:-}"; do
    if [[ -n "$name" ]]; then
        build_arguments+=("--target" "$name")
    fi
done
cmake "${build_arguments[@]}"

echo
echo "Output directory: $build_dir"
