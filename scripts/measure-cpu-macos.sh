#!/bin/bash
set -euo pipefail

# Measure an existing process without launching, modifying, or stopping it.
if [[ $(uname -s) != Darwin || $# -lt 1 || $# -gt 3 ]]; then
    echo "Usage (macOS): bash scripts/measure-cpu-macos.sh PID [seconds=15] [max-cpu-percent]" >&2
    exit 2
fi
pid=$1
duration=${2:-15}
budget=${3:-}
if [[ ! $pid =~ ^[1-9][0-9]*$ || ! $duration =~ ^[1-9][0-9]*$ ]]; then
    echo "PID and duration must be positive integers" >&2
    exit 2
fi
if [[ -n $budget && ! $budget =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    echo "CPU budget must be a non-negative number" >&2
    exit 2
fi

snapshot() {
    LC_ALL=C ps -p "$pid" -o lstart=,time= | awk '
        NF >= 6 {
            identity = $1 " " $2 " " $3 " " $4 " " $5
            n = split($6, parts, ":")
            seconds = n == 3 ? parts[1]*3600 + parts[2]*60 + parts[3] : parts[1]*60 + parts[2]
            printf "%s|%.2f\n", identity, seconds
        }'
}

before=$(snapshot) || {
    echo "Cannot read process: $pid" >&2
    exit 2
}
if [[ -z $before ]]; then
    echo "Process not found: $pid" >&2
    exit 2
fi
start=$(perl -MTime::HiRes=clock_gettime,CLOCK_MONOTONIC -e 'printf "%.6f", clock_gettime(CLOCK_MONOTONIC)')
sleep "$duration"
after=$(snapshot) || {
    echo "Process exited or cannot be read: $pid" >&2
    exit 2
}
end=$(perl -MTime::HiRes=clock_gettime,CLOCK_MONOTONIC -e 'printf "%.6f", clock_gettime(CLOCK_MONOTONIC)')
if [[ -z $after || ${before%|*} != "${after%|*}" ]]; then
    echo "Process exited or PID was reused during measurement" >&2
    exit 2
fi
awk -v before="${before##*|}" -v after="${after##*|}" -v start="$start" -v end="$end" -v budget="$budget" '
    BEGIN {
        cpu = 100 * (after-before) / (end-start)
        printf "CPU %.2f%% over %.3f seconds (100%% = one core)\n", cpu, end-start
        if (budget != "") {
            printf "Budget %.2f%%: %s\n", budget, (cpu > budget ? "FAIL" : "PASS")
            exit (cpu > budget ? 1 : 0)
        }
    }'
