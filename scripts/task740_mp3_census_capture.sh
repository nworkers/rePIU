#!/usr/bin/env bash
# Task 740. Captures one pumpitea run with the PIU10 MP3 pipeline census, to
# tell "the arrows stall because the frame-sync toggles or the feed move in
# device-buffer steps" from "the PCM queue ran dry" on the host at hand.
#
# The attract demo plays no MP3: play a song the usual way (SERVICE for
# credits, a pad to start, choose a song) and let it run for twenty seconds.
#
# What the output answers:
#   [repiu-piu10-mp3-census] one line every 20 ms while the run lasts:
#       received  guest bytes accepted so far (a byte-counted song position)
#       decoded   MPEG frames decoded
#       queued_ms PCM queued ahead of the device
#       lag_ms    playback clock behind the device's pulled count (at most one
#                 device buffer while the device keeps time)
#       toggles   frame-sync toggles handed to the guest
#       multi     worker calls that toggled more than once (a 240 Hz poller
#                 reads an even number as no change) -- should stay 0
#       starvation decoder found no compressed input while playing
#       pcm_empty queue found empty just before a refill mid-song -- a gap on
#                 the speaker; should stay 0
#   final report line
#       PIU10 MP3 received/dropped/decoded/starvation/toggles/multi/pcm-empty/
#       pcm-low-water-bytes/device-buffer-frames
#
# Optional: SDL_AUDIO_DEVICE_SAMPLE_FRAMES=<frames> forces the SDL device
# buffer (2048 = 46 ms at 44.1 kHz) to reproduce a host with a larger buffer.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 1

label="${1:-task740}"
rom_set="${2:-pumpitea}"
binary="${3:-./build/linux_x64_release/repiu}"
timeout_ms="${4:-90000}"

if [ ! -x "${binary}" ]; then
    echo "engine binary not found: ${binary}" >&2
    exit 1
fi

mkdir -p build
ulimit -c 0
export SDL_VIDEO_DRIVER="${SDL_VIDEO_DRIVER:-x11}"
export REPIU_STALL_TIMEOUT_MS=0
export REPIU_EXECUTION_TIMEOUT_MS="${timeout_ms}"
export REPIU_PIU10_MP3_CENSUS_MS=20
export REPIU_GLIDE_FRAME_RATE_LOG=1

log="build/${label}.err.log"
"${binary}" "${rom_set}" > /dev/null 2> "${log}"
status=$?

echo "exit=${status} faults=$(grep -ac 'repiu-fault\]' "${log}") census=$(grep -ac 'repiu-piu10-mp3-census' "${log}")"
grep -a "PIU10 MP3 received\|timer tick delivery backlog" "${log}"
echo "per second (song window): ticks toggles multi bytes min-queue-ms max-lag-ms"
grep -a "repiu-piu10-mp3-census" "${log}" | awk '
{
    for (i = 2; i <= NF; i++) { split($i, a, "="); v[a[1]] = a[2] }
    s = int(v["elapsed_ms"] / 1000)
    if (s != cs) {
        if (have && v["received"] - b0 > 0)
            printf "%ds ticks=%d toggles=%d multi=%d bytes=%d minq=%.0f maxlag=%.0f\n",
                cs, v["ticks_injected"] - t0, v["toggles"] - g0, v["multi"] - m0,
                v["received"] - b0, mq, xl
        cs = s; have = 1
        t0 = v["ticks_injected"]; g0 = v["toggles"]; m0 = v["multi"]; b0 = v["received"]
        mq = 1e9; xl = 0
    }
    q = v["queued_ms"] + 0; if (q < mq) mq = q
    l = v["lag_ms"] + 0; if (l > xl) xl = l
}'
