#!/usr/bin/env bash
# Task 734. Captures one interactive pumpit2a run with the instruments that
# separate "the game's main loop stopped" from "the main loop still draws but
# its state never advances".
#
# Play to the song-select screen the usual way (SERVICE to add credits, then a
# pad to start), wait until the screen stops reacting, leave it there for
# another twenty to thirty seconds, then close the window.
#
# What each instrument answers:
#   REPIU_GLIDE_FRAME_RATE_LOG      one `[repiu-frame-rate]` line per second, and
#                                   only when a frame was presented. Lines that
#                                   keep coming after the freeze mean the render
#                                   loop is alive; lines that stop mean it is not.
#   REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE
#                                   one `[repiu-x64-sample]` line per census
#                                   sample, with elapsed_ms and guest_eip, so the
#                                   frozen stretch can be profiled on its own.
#   REPIU_AOT_CACHE_MAP_TRACE       prints, on the way out, which guest address a
#                                   code-cache address belongs to. The default is
#                                   where shutdown recovery found the guest in
#                                   every run so far (0x20253CAD, or 0x20253C46 --
#                                   the same translated block).
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 1

label="${1:-task734}"
cache_address="${2:-0x20253CAD}"
rom_set="${3:-pumpit2a}"
binary="${4:-./build/linux_x64_debug/repiu}"

if [ ! -x "${binary}" ]; then
    echo "engine binary not found: ${binary}" >&2
    exit 1
fi

mkdir -p build
ulimit -c 0

err="build/${label}.err.log"
out="build/${label}.out.log"

REPIU_GLIDE_FRAME_RATE_LOG=1 \
REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE=1 \
REPIU_AOT_CACHE_MAP_TRACE="${cache_address}" \
REPIU_GUEST_POSITION_CENSUS=1 \
REPIU_GUEST_POSITION_CENSUS_MS=20 \
REPIU_GUEST_POSITION_CENSUS_DUMP="build/${label}_census.txt" \
REPIU_MSCDEX_COMMAND_TRACE=1 \
REPIU_MSCDEX_COMMAND_TRACE_DUMP="build/${label}_mscdex.txt" \
REPIU_CD_AUDIO_POSITION_CENSUS=1 \
REPIU_CD_AUDIO_POSITION_CENSUS_MS=100 \
REPIU_CD_AUDIO_POSITION_CENSUS_DUMP="build/${label}_cd.txt" \
    "${binary}" "${rom_set}" >"${out}" 2>"${err}"
status=$?

echo "=== exit=${status}"
echo "=== log: ${err}"
echo "=== dumps: build/${label}_census.txt build/${label}_mscdex.txt build/${label}_cd.txt"
echo "--- frame-rate lines (first 3, last 3) ---"
grep -a "repiu-frame-rate" "${err}" | head -3
grep -a "repiu-frame-rate" "${err}" | tail -3
echo "--- frame-rate line count: $(grep -ac "repiu-frame-rate" "${err}") ---"
grep -a "repiu-aot-cache-map" "${err}"
grep -a "repiu-shutdown] reason=" "${err}" | tail -1
