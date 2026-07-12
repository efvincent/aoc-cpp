#!/usr/bin/env bash
set -euo pipefail

# Benchmark wrapper for build/aoc_worker_<mode>.
# Usage:
#   ./bench_nanos.sh N BUILD year day [part]
#
# The first argument N is the iteration count.
# The second argument BUILD is a build mode (debug, release, lto, instrument)
# or an explicit binary path.
# Remaining args are passed to the program as-is.
# A CSV log line is appended per benchmark run.

LOG_FILE="${AOC_BENCH_LOG:-bench_results.csv}"
DEFAULT_BUILD_MODE="debug"

resolve_bin() {
  local build_spec="$1"
  case "$build_spec" in
    debug|release|lto|instrument)
      echo "build/aoc_worker_${build_spec}"
      ;;
    "")
      echo "build/aoc_worker_${DEFAULT_BUILD_MODE}"
      ;;
    *)
      echo "$build_spec"
      ;;
  esac
}

usage() {
  echo "Usage: $0 N BUILD <program-args...>"
  echo "BUILD can be: debug, release, lto, instrument, or a binary path"
  echo "Example with part:    $0 1000 debug 2015 2 1"
  echo "Example without part: $0 1000 release 2015 2"
}

if [[ $# -lt 4 ]]; then
  usage
  exit 1
fi

N="$1"
BUILD_SPEC="$2"
shift
shift

if [[ ! "$N" =~ ^[0-9]+$ ]] || [[ "$N" -eq 0 ]]; then
  echo "Error: N must be a positive integer."
  exit 1
fi

BIN="$(resolve_bin "$BUILD_SPEC")"

if [[ ! -x "$BIN" ]]; then
  echo "Error: benchmark binary not found or not executable: $BIN"
  echo "Build it first, for example: make ${BUILD_SPEC}"
  exit 1
fi

args=("$@")

# For AoC CLI convention, log year/day/part where part may be omitted.
year=""
day=""
part=""
if [[ ${#args[@]} -ge 1 ]]; then year="${args[0]}"; fi
if [[ ${#args[@]} -ge 2 ]]; then day="${args[1]}"; fi
if [[ ${#args[@]} -ge 3 ]]; then part="${args[2]}"; fi

# Create log with header once.
if [[ ! -f "$LOG_FILE" ]]; then
  echo "timestamp,iterations,build,year,day,part,total_ns,avg_ns" > "$LOG_FILE"
fi

total_ns=0

for ((i = 1; i <= N; i++)); do
  start_ns=$(date +%s%N)
  "$BIN" "${args[@]}" >/dev/null
  end_ns=$(date +%s%N)

  run_ns=$((end_ns - start_ns))
  total_ns=$((total_ns + run_ns))
done

avg_ns=$((total_ns / N))

timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
printf '%s,%s,%s,%s,%s,%s,%s,%s\n' \
  "$timestamp" "$N" "$BUILD_SPEC" "$year" "$day" "$part" "$total_ns" "$avg_ns" >> "$LOG_FILE"

echo "Benchmark complete"
echo "binary:    $BIN"
echo "build:     $BUILD_SPEC"
echo "runs:      $N"
echo "args:      ${args[*]}"
echo "total_ns:  $total_ns"
echo "avg_ns:    $avg_ns"
echo "log_file:  $LOG_FILE"
