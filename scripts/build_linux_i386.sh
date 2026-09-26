#!/usr/bin/env bash
# Task 501. Builds the Linux host as 32-bit x86: the core, the execution
# engine, the loader (`repiu`), the launcher, the probes and the tools.
#
# The architecture is not a preference. The guest's 32-bit x86 code runs
# natively in the host process, exactly as it does under the Win32 host, so the
# host itself has to be a 32-bit process. (The x86-64 host in
# build_linux_x64.sh runs the guest through the long-mode code cache instead.)
#
# Task 739 brought this script level with the x64 one: `--build-dir` keeps two
# configurations apart, the SDL console switch is passed both ways, and the
# header no longer describes the Task 501 state in which only the core and its
# probe existed on Linux.
set -euo pipefail

configuration="Debug"
targets=()
headless=0
build_directory=""

usage()
{
    cat <<'USAGE'
usage: build_linux_i386.sh [--config Debug|Release|RelWithDebInfo|MinSizeRel]
                           [--build-dir PATH] [--target NAME]... [--headless]
Builds into build/linux_i386 unless --build-dir names another directory. With no
--target, every default target is built. --headless lets SDL configure on a host
without X11/Wayland development packages; it suits the core and its probes but
not the launcher. (SDL still builds every desktop driver it finds, so on a host
that has the packages the flag changes nothing.)

--build-dir is what keeps two configurations apart. This is a single-config
generator, so a tree holds exactly one CMAKE_BUILD_TYPE: pointing --config at a
directory configured the other way reconfigures it in place and discards the
build that was there. Give the second configuration its own directory --
build/linux_i386_release next to build/linux_i386, say -- when you want both.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            configuration="${2:?--config needs a value}"
            shift 2
            ;;
        --build-dir)
            build_directory="${2:?--build-dir needs a value}"
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
build_dir="${build_directory:-$root/build/linux_i386}"

# Named rather than left to be discovered: reconfiguring a tree to the other
# build type throws away everything already compiled there, and the message that
# says so scrolls past inside CMake's output.
if [[ -f "$build_dir/CMakeCache.txt" ]]; then
    existing="$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' \
        "$build_dir/CMakeCache.txt")"
    if [[ -n "$existing" && "$existing" != "$configuration" ]]; then
        cat >&2 <<REconfig
$build_dir is configured as $existing and this run asks for $configuration.

A single-config generator holds one build type per directory, so continuing
would reconfigure that tree and rebuild it from nothing. Use --build-dir to give
this configuration its own directory:

    scripts/build_linux_i386.sh --config $configuration \
        --build-dir "$root/build/linux_i386_$(echo "$configuration" | tr '[:upper:]' '[:lower:]')"

REconfig
        exit 1
    fi
fi

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

# The same shape of quiet failure, one layer up: on a Wayland desktop there are
# no server-side decorations, so the client draws its own and SDL delegates that
# to libdecor. Without libdecor-0-dev:i386 SDL compiles its Wayland driver with
# the support left out entirely, and the game opens a window with no title bar
# and nothing to drag or close it by. Nothing reports this -- the window is
# simply bare. Observed on Ubuntu 25.10 (GNOME/Wayland), where the amd64
# libdecor was installed and the i386 one was not.
if [[ $headless -eq 0 ]] && ! ls /usr/lib/i386-linux-gnu/libdecor-0.so \
        /lib/i386-linux-gnu/libdecor-0.so > /dev/null 2>&1; then
    cat >&2 <<'DECOR'
Note: no 32-bit libdecor, so on a Wayland desktop the window will open without a
title bar or borders:

    sudo apt install -y libdecor-0-dev:i386 libdecor-0-0:i386 libdecor-0-plugin-1-gtk:i386

SDL caches the lookup, so installing it afterwards needs the cached answer
cleared -- cheaper than discarding the build directory:

    cmake -U "PC_LIBDECOR*" -U HAVE_LIBDECOR_H -S . -B build/linux_i386

Running with SDL_VIDEODRIVER=x11 is the workaround that needs no rebuild.

DECOR
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
# Task 739. The console-build switch is passed both ways rather than only when
# asked for: CMake keeps the previous answer in the cache, so a tree once
# configured --headless stayed that way on every later plain run and nothing
# said so. (All the switch does is let SDL configure without X11/Wayland
# development packages -- SDL still builds every desktop driver it finds.)
sdl_options=(-DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF)
if [[ $headless -ne 0 ]]; then
    sdl_options+=(-DSDL_UNIX_CONSOLE_BUILD=ON)
else
    sdl_options+=(-DSDL_UNIX_CONSOLE_BUILD=OFF)
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

# The job count is named rather than left to `--parallel` alone.
#
# A bare `--parallel` passes `-j` with no number to make, which means *unlimited*
# jobs: make starts every target whose prerequisites are ready. On a four-core VM
# that was measured at 58 concurrent cc1plus processes, and the engine's larger
# translation units take well over a gigabyte each in Debug -- so the build did
# not merely run slowly, it exhausted memory and took the whole WSL VM down with
# it, three times, each looking like an unrelated failure.
#
# CMAKE_BUILD_PARALLEL_LEVEL does not help by itself: CMake consults it only when
# `--parallel` is absent, so setting it while the option is passed has no effect
# at all. Here it selects the count when set, and `nproc` is the default.
jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc 2>/dev/null || echo 2)}"
build_arguments=("--build" "$build_dir" "--parallel" "$jobs")
for name in "${targets[@]:-}"; do
    if [[ -n "$name" ]]; then
        build_arguments+=("--target" "$name")
    fi
done
cmake "${build_arguments[@]}"

echo
echo "Output directory: $build_dir"
