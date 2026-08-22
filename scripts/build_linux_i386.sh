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
headless=0

usage()
{
    cat <<'USAGE'
usage: build_linux_i386.sh [--config Debug|Release|RelWithDebInfo|MinSizeRel]
                           [--target NAME]... [--headless]
Builds into build/linux_i386. With no --target, every default target is built.
--headless drops SDL desktop support, which suits the core and its probes but
not the launcher.
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
        --headless)
            headless=1
            shift
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

# SDL3 needs X11 or Wayland development packages to configure. --headless skips
# that requirement for the core and its probes, which open no window; the
# launcher needs the real desktop packages.
#
# XSCRNSAVER and XTEST are switched off rather than installed: both are optional
# X11 extensions this project never uses, for inhibiting the screen saver and
# simulating input, and every extension left on is one more 32-bit package an
# operator has to hunt down.
sdl_options=(-DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF)
if [[ $headless -ne 0 ]]; then
    sdl_options+=(-DSDL_UNIX_CONSOLE_BUILD=ON)
fi

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$configuration" \
    "${sdl_options[@]}" \
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
