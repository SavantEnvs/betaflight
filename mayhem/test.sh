#!/usr/bin/env bash
#
# mayhem/test.sh — RUN Betaflight's own gtest unit tests for the fuzzed code (built by mayhem/build.sh
# into obj/test). Each binary asserts known-input -> known-output values; the pinned counts below make a
# neutered or silent binary (which prints nothing) a failure rather than a pass.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


# <binary name> <pinned number of tests that must pass>
TESTS=(
    "rx_sumd_unittest 12"
    "rx_xbus_unittest 4"
    "rx_ibus_unittest 12"
    "rx_crsf_unittest 13"
    "msp_serial_unittest 12"
    "encoding_unittest 2"
    "huffman_unittest 3"
    "uvarint_unittest 3"
    "telemetry_ibus_unittest 17"
    "blackbox_encoding_unittest 6"
    "gps_conversion_unittest 2"
)

passed=0
failed=0
for entry in "${TESTS[@]}"; do
    read -r name want <<<"$entry"
    bin="$SRC/obj/test/$name/$name"
    if [ ! -x "$bin" ]; then
        echo "MISSING test binary: $bin" >&2
        failed=$((failed + want))
        continue
    fi
    out=$("$bin" 2>&1)
    rc=$?
    echo "$out" | tail -n 5
    got=$(printf '%s\n' "$out" | sed -n 's/^\[  PASSED  \] \([0-9][0-9]*\) tests\?\..*/\1/p' | tail -n 1)
    got=${got:-0}
    if [ "$rc" -eq 0 ] && [ "$got" -eq "$want" ] && ! printf '%s\n' "$out" | grep -q '^\[  FAILED  \]'; then
        passed=$((passed + got))
    else
        echo "FAILED $name: rc=$rc, passed $got of $want pinned" >&2
        passed=$((passed + got))
        failed=$((failed + (want > got ? want - got : 1)))
    fi
done

emit_ctrf "gtest" "$passed" "$failed" 0
