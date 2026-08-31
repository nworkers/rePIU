#!/usr/bin/env bash
# Task 549. Builds the Linux host as x86-64.
#
# This is not the i386 script with one flag removed, and the architecture is not
# a preference here either. On i386 the guest's 32-bit code runs natively in the
# host process, so the host has to be a 32-bit process. On x86-64 it cannot:
# Task 544 fences the native guest entry closed and Task 546 records why. What
# this configuration builds today is therefore the platform layer, the probes,
# and the x64 execution contracts being brought up one at a time -- not a host
# that runs a guest.
#
# Keep it buildable anyway. Every x64 barrier found so far (the telemetry ABI in
# 543, the guest entry in 544, the i386 assembly in 545) was found by compiling,
# and a configuration nobody can configure is one nobody measures.
set -euo pipefail

configuration="Debug"
targets=()
headless=0
build_directory=""

usage()
{
    cat <<'USAGE'
usage: build_linux_x64.sh [--config Debug|Release|RelWithDebInfo|MinSizeRel]
                          [--build-dir PATH] [--target NAME]... [--headless]
Builds into build/linux_x64 unless --build-dir names another directory. With no
--target, every default target is built. --headless drops SDL desktop support,
which suits the core and its probes but not the launcher.

--build-dir is what keeps two configurations apart. This is a single-config
generator, so a tree holds exactly one CMAKE_BUILD_TYPE: pointing --config at a
directory configured the other way reconfigures it in place and discards the
build that was there. Give the second configuration its own directory --
build/linux_x64_release next to build/linux_x64, say -- when you want both.

Keeping both is worth the disk. Task 549 found a fault handler that worked at
-O0 and was optimised away at -O2, and it was only visible because a Debug tree
and a Release tree existed at the same time to compare.
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
build_dir="${build_directory:-$root/build/linux_x64}"

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

    scripts/build_linux_x64.sh --config $configuration \\
        --build-dir "$root/build/linux_x64_$(echo "$configuration" | tr '[:upper:]' '[:lower:]')"

REconfig
        exit 1
    fi
fi

# The i386 script checks for a working `-m32` toolchain because gcc-multilib is
# the package an operator most often lacks. Here the native compiler is the
# toolchain, so the check that is worth making is the opposite one: that this
# host really is x86-64, because the same script on aarch64 would configure a
# tree whose x64 assembly cannot assemble.
machine="$(uname -m)"
if [[ "$machine" != "x86_64" ]]; then
    cat >&2 <<NOTX64
This host reports \`$machine\`. The x64 configuration assembles SysV AMD64
sources and is only meaningful on x86_64.

NOTX64
    exit 1
fi

# XSCRNSAVER and XTEST are switched off for the same reason as on i386: both are
# optional X11 extensions this project never uses.
sdl_options=(-DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF)
if [[ $headless -ne 0 ]]; then
    sdl_options+=(-DSDL_UNIX_CONSOLE_BUILD=ON)
fi

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$configuration" \
    "${sdl_options[@]}"

# The job count is named rather than left to `--parallel` alone, for the reason
# the i386 script records at length: a bare `--parallel` passes `-j` with no
# number to make, which means unlimited, and the engine's larger Debug
# translation units took a four-core VM down three times.
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
