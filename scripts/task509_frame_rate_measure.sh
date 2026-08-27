#!/usr/bin/env bash
# Task 509. Measures how fast a run actually draws, from the line every run
# prints on the way out.
#
# The rate is frames over the span since the FIRST presented frame, not over the
# budget: pumpit1 spends roughly its first forty-five seconds decoding start-up
# assets with nothing on screen. See docs/guides/execution-frame-rate-measurement.md.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 1

runs="${1:-3}"
budget_ms="${2:-90000}"
label="${3:-task509}"
rom_set="${4:-pumpit1}"

ulimit -c 0

for ((run = 1; run <= runs; ++run)); do
    err="build/${label}-run${run}.err"
    # vsync off, or what gets measured is the display rather than the engine.
    REPIU_STALL_TIMEOUT_MS=0 \
    REPIU_EXECUTION_TIMEOUT_MS="${budget_ms}" \
    REPIU_GLIDE_SWAP_INTERVAL=0 \
        ./build/linux_i386/repiu "${rom_set}" \
        >"build/${label}-run${run}.out" 2>"${err}" &
    wait $!
    status=$?
    line="$(grep -a "repiu-shutdown] reason=" "${err}" | tail -1)"
    frames="$(sed -n 's/.* frames=\([0-9]*\).*/\1/p' <<<"${line}")"
    span="$(sed -n 's/.* span_ms=\([0-9]*\).*/\1/p' <<<"${line}")"
    fps="n/a"
    if [ -n "${frames:-}" ] && [ -n "${span:-}" ] && [ "${span:-0}" -gt 0 ]; then
        fps="$(awk -v f="${frames}" -v s="${span}" 'BEGIN { printf "%.2f", f * 1000.0 / s }')"
    fi
    echo "=== run=${run} exit=${status} frames=${frames:-?} span_ms=${span:-?} fps=${fps}"
done
