#!/usr/bin/env bash
# Runs every ROM set profile for a fixed time and summarises whether it starts
# and progresses. Works from a Linux shell (WSL) and from Git Bash on Windows.
#
# usage: scripts/survey_romsets.sh <binary> <tag> [seconds] [profile ...]
#   scripts/survey_romsets.sh build/linux_x64_release/repiu linux-x64
#   scripts/survey_romsets.sh build/win32_x86_debug/Debug/repiu.exe win32
#
# Output: build/survey/<tag>/<profile>.err.log, <profile>.out.log and
# build/survey/<tag>/summary.txt. One run at a time; do not start a second host
# while this is running.
set -u
cd "$(dirname "$0")/.." || exit 1
binary=${1:?binary}; tag=${2:?tag}; seconds=${3:-60}
shift 2; [ $# -gt 0 ] && shift
profiles=("$@")
if [ ${#profiles[@]} -eq 0 ]; then
  profiles=(pumpit1 pumpit2 pumpit2a pumpit3 pumpit3a pumpito pumpitc pumpitpc
            pumpitpr pumpitpru pumpite pumpitea pumpitpx pumpit8 pumpitp2
            pumpipx2 pumpipx2p pumpitp3 pumpitp3a pumpipx3 pumpipx3a pumpipx3b)
fi
out=build/survey/$tag
mkdir -p "$out"
summary=$out/summary.txt
: > "$summary"
# x11 on Linux unless the caller chose: WSLg's wayland socket is not where
# XDG_RUNTIME_DIR points, and the survey should not depend on that.
if [ "$(uname -s)" = Linux ]; then
  export SDL_VIDEO_DRIVER=${SDL_VIDEO_DRIVER:-x11}
fi
export REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=$((seconds * 1000))
# Issue #45: measure in a window even when cfg/repiu.ini stores fullscreen;
# the caller can still ask for it.
export REPIU_GLIDE_FULLSCREEN=${REPIU_GLIDE_FULLSCREEN:-0}
export REPIU_GLIDE_FRAME_RATE_LOG=1 REPIU_GLIDE_PIXEL_DIAG=1
export REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS=5000 REPIU_DOS_ASSET_TRACE=1
export REPIU_INPUT_SCRIPT=scripts/input_scripts/survey_generic.txt
for profile in "${profiles[@]}"; do
  err=$out/$profile.err.log; log=$out/$profile.out.log
  started=$(date +%s)
  timeout $((seconds + 150)) "$binary" "$profile" > "$log" 2> "$err"
  code=$?
  wall=$(( $(date +%s) - started ))
  frames=$(grep -a -o 'frames=[0-9]*' "$err" | tail -1 | cut -d= -f2)
  faults=$(grep -a -c 'repiu-fault\]' "$err")
  untranslatable=$(grep -a -c 'repiu-x64-untranslatable' "$err")
  inputs=$(grep -a -c '^\[repiu-input\] ' "$err")
  opens_ok=$(grep -a -c '^\[repiu-asset\] OK ' "$err")
  opens_fail=$(grep -a -c '^\[repiu-asset\] FAIL' "$err")
  scenes=$(grep -a '^\[repiu-scene\]' "$err" | sed 's/.*non_black=\([0-9]*\).*grid=\([0-9A-F]*\).*/\1:\2/')
  scene_count=$(printf '%s\n' "$scenes" | grep -c .)
  scene_distinct=$(printf '%s\n' "$scenes" | grep . | cut -d: -f2 | sort -u | wc -l)
  scene_black=$(printf '%s\n' "$scenes" | grep -c '^0:')
  fps_tail=$(grep -a '^\[repiu-frame-rate\]' "$err" | tail -20 | sed 's/.*fps=\([0-9.]*\).*/\1/' | sort -n | awk '{v[NR]=$1} END {if (NR) printf "%s/%s/%s", v[1], v[int((NR+1)/2)], v[NR]; else printf "-"}')
  {
    echo "=== $profile exit=$code wall=${wall}s frames=${frames:-0} faults=$faults untranslatable=$untranslatable inputs=$inputs"
    echo "    fps(last 20 s) min/median/max=$fps_tail  scenes=$scene_count distinct=$scene_distinct black=$scene_black  asset opens ok/fail=$opens_ok/$opens_fail"
    printf '%s\n' "$scenes" | grep . | tr '\n' ' ' | sed 's/^/    scene: /'; echo
    grep -a 'repiu-fault\] \|repiu-x64-untranslatable\|repiu-shutdown\] reason\|\[   error\]\|\[critical\]' "$err" "$log" | sed 's/^[^:]*://' | cut -c1-260 | head -6 | sed 's/^/    ! /'
    grep -a 'PIU10 MP3 received\|CD audio\|timer tick delivery\|guest cli hold\|swap wait ticks' "$err" "$log" | sed 's/^[^:]*://; s/.*\[loader\] //' | cut -c1-200 | head -6 | sed 's/^/    . /'
    grep -a '^\[repiu-asset\] OK ' "$err" | tail -3 | sed 's/ -> .*//' | cut -c1-120 | sed 's/^/    last open: /'
  } >> "$summary"
done
echo "done: $summary"
