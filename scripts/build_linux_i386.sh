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

# A warning rather than a failure: a silent build is still a usable build, and an
# operator who only wants to check that the guest runs should not be stopped for
# it. Note that SDL caches this decision, so installing the package later means
# discarding the build directory -- reconfiguring an existing one keeps the
# answer it already found.
if [[ $headless -eq 0 ]] && ! ls /usr/lib/i386-linux-gnu/libpulse.so         /lib/i386-linux-gnu/libpulse.so > /dev/null 2>&1; then
    cat >&2 <<'AUDIO'
Note: no 32-bit libpulse, so SDL will build with ALSA only and the game will be
silent on any host that routes audio through PulseAudio or PipeWire:

    sudo apt install -y libpulse-dev:i386

Then remove build/linux_i386 before rebuilding; SDL caches which audio drivers
it found at configure time.

AUDIO
fi

# SDL3 needs X11 or Wayland development packages to configure. --headless skips
# that requirement for the core and its probes, which open no window; the
# launcher needs the real desktop packages.
#
# XSCRNSAVER and XTEST are switched off rather than installed: both are optional
# X11 extensions this project never uses, for inhibiting the screen saver and
# simulating input, and every extension left on is one more 32-bit package an
# operator has to hunt down.
#
# Audio is the one that fails quietly. SDL compiles the drivers it can find at
# configure time, so without libpulse-dev:i386 it keeps only ALSA -- which then
# reports "Couldn't open audio device" on any host that routes sound through
# PulseAudio or PipeWire, WSL among them. The build still succeeds and the game
# still runs; it just never makes a sound. The check below names the package
# rather than leaving that to be rediscovered.
sdl_options=(-DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF)
if [[ $headless -ne 0 ]]; then
    sdl_options+=(-DSDL_UNIX_CONSOLE_BUILD=ON)
fi

# SDL finds its optional dependencies with pkg-config, and pkg-config's default
# search path on an amd64 host names only the 64-bit directory. The 32-bit
# packages install their .pc files somewhere it never looks:
#
#     /usr/lib/i386-linux-gnu/pkgconfig/libpulse.pc      <- installed here
#     /usr/lib/x86_64-linux-gnu/pkgconfig                <- searched here
#
# So `apt install libpulse-dev:i386` alone changes nothing: SDL reports that the
# package is not found while the library sits on disk. Naming the directory here
# is what makes an installed 32-bit package visible to a 32-bit build. Prepended
# rather than replacing the default, so anything found the usual way still is.
export PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$configuration" \
    "${sdl_options[@]}" \
    -DCMAKE_C_FLAGS=-m32 \
    -DCMAKE_CXX_FLAGS=-m32 \
    -DCMAKE_ASM_FLAGS=-m32 \
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
