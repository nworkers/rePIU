#!/usr/bin/env bash
# Task 503. How far the execution engine has come on Linux, as a number.
#
# Every source under src/platform/win32 is compiled -- syntax only, nothing
# linked -- with the flags the Linux build already uses for repiu_exe, and the
# script reports how many succeed. That count is the port's progress metric:
# every sub-stage of Task 503d records it, and a sub-stage that does not move it
# has to say why.
#
# The flags come out of the configured build tree rather than being written down
# here, because a list of include directories copied into a script is a list
# that goes stale the first time CMakeLists.txt changes. The cost is that the
# tree has to exist; scripts/build_linux_i386.sh creates it.
#
# Nothing is linked on purpose. Linking asks a different question -- which
# symbols exist -- and the sources are still in the `if(WIN32)` list, so most of
# them have no object to link against yet.
set -euo pipefail

configuration_dir="build/linux_i386"
subject_dir="src/platform/win32"
log_dir=""
show_errors=0

usage()
{
    cat <<'USAGE'
usage: measure_linux_engine_port.sh [--build-dir DIR] [--source-dir DIR]
                                    [--log-dir DIR] [--errors]

Compiles every .cpp under the source directory with the Linux i386 flags of the
configured build tree and reports "compiled=N failed=M total=T".

  --build-dir DIR   configured CMake tree to take flags from (default
                    build/linux_i386)
  --source-dir DIR  what to measure (default src/platform/win32)
  --log-dir DIR     keep the compiler output of each failure here
  --errors          print the first errors of each failing source
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            configuration_dir="${2:?--build-dir needs a value}"
            shift 2
            ;;
        --source-dir)
            subject_dir="${2:?--source-dir needs a value}"
            shift 2
            ;;
        --log-dir)
            log_dir="${2:?--log-dir needs a value}"
            shift 2
            ;;
        --errors)
            show_errors=1
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
cd "$root"

flags_file="$configuration_dir/CMakeFiles/repiu_exe.dir/flags.make"
if [[ ! -f "$flags_file" ]]; then
    cat >&2 <<NEEDS
No configured Linux build tree at $configuration_dir.

    scripts/build_linux_i386.sh --headless --target repiu_core_probe

creates one. The flags this measurement uses are read from it rather than
duplicated here.
NEEDS
    exit 1
fi

# CMake writes one assignment per line. Everything after the first `=` is the
# value, and `sed` keeps it verbatim -- the include list contains no quoting the
# shell has to preserve, and re-quoting it would be the way to break it.
read_flag()
{
    sed -n "s/^$1 = //p" "$flags_file" | head -1
}

defines="$(read_flag CXX_DEFINES)"
includes="$(read_flag CXX_INCLUDES)"
flags="$(read_flag CXX_FLAGS)"

if [[ -z "$includes" ]]; then
    echo "could not read CXX_INCLUDES from $flags_file" >&2
    exit 1
fi

if [[ -n "$log_dir" ]]; then
    mkdir -p "$log_dir"
fi

compiled=0
failed=0
while IFS= read -r source; do
    output="$( ( eval "\${CXX:-c++} $flags $defines $includes -fsyntax-only \"$source\"" ) 2>&1 )" \
        && status=0 || status=$?
    if [[ $status -eq 0 ]]; then
        compiled=$((compiled + 1))
        continue
    fi
    failed=$((failed + 1))
    # Both counts are reported: `error:` lines say how much is wrong, while a
    # fatal error -- a missing header -- stops the compiler and hides whatever
    # was behind it. Task 503d-15 and 503d-16 both found the second kind
    # standing in front of far less than its position suggested.
    error_count="$(grep -c 'error:' <<<"$output" || true)"
    echo "FAIL $source ($error_count errors)"
    if [[ -n "$log_dir" ]]; then
        printf '%s\n' "$output" > "$log_dir/$(tr '/' '_' <<<"$source").log"
    fi
    if [[ $show_errors -ne 0 ]]; then
        grep 'error:' <<<"$output" | head -5 | sed 's/^/    /' || true
    fi
done < <(find "$subject_dir" -name '*.cpp' | sort)

echo "compiled=$compiled failed=$failed total=$((compiled + failed))"
[[ $failed -eq 0 ]]
