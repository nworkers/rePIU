#!/usr/bin/env bash
# Task 508. Repeats the Linux shutdown check and reports how each run ended.
#
# The point is the refused-recovery path: a run whose guest thread is still
# inside host code when the budget expires cannot be recovered, and only that
# path exercises what 507 and 508 changed. Reaching it reliably needs a budget
# long enough for the guest to be in its render loop -- a short budget catches
# it in the start-up asset decode, where recovery almost always succeeds. See
# docs/guides/linux-shutdown-check.md section 2.4.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 1

runs="${1:-3}"
budget_ms="${2:-60000}"
label="${3:-task508}"
rom_set="${4:-pumpit1}"

# A core dump on this path is the finding, not something to collect: the shell
# reports the signal either way, and the dumps are large.
ulimit -c 0

refused=0
trapped=0
for ((run = 1; run <= runs; ++run)); do
    out="build/${label}-run${run}.out"
    err="build/${label}-run${run}.err"
    REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS="${budget_ms}" \
        ./build/linux_i386/repiu "${rom_set}" >"${out}" 2>"${err}" &
    wait $!
    status=$?
    note=""
    if [ "${status}" -gt 128 ]; then
        note=" signal=$((status - 128))"
        if [ "$((status - 128))" -eq 5 ]; then
            trapped=$((trapped + 1))
        fi
    fi
    if grep -aq "stopped=0" "${err}"; then
        refused=$((refused + 1))
        note="${note} refused"
    fi
    echo "=== run=${run} exit=${status}${note}"
    grep -a "repiu-shutdown" "${err}" | tail -12
done

echo "=== ${label}: runs=${runs} refused=${refused} sigtrap=${trapped}"
