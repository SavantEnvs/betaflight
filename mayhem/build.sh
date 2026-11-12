#!/usr/bin/env bash
#
# mayhem/build.sh — builds the fuzz harnesses (sanitized project objects + libFuzzer and
# -standalone binaries) and, separately, the upstream gtest unit tests with their normal flags.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC=${SRC:-/mayhem}
cd "$SRC"

MAIN=$SRC/src/main
UNIT=$SRC/src/test/unit
HARN=$SRC/mayhem/harnesses
OBJ=$SRC/obj/fuzz
OUT=$SRC

# The firmware's host unit-test configuration (src/test/unit/platform.h shadows the target's).
COMMON=(-DUNIT_TEST -O1 -w -I"$UNIT" -I"$MAIN" $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link)
CFLAGS=(-std=gnu99 -D_GNU_SOURCE "${COMMON[@]}")
CXXFLAGS=(-std=gnu++14 -D_GNU_SOURCE "${COMMON[@]}")

mkdir -p "$OBJ"
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/standalone_main.o"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OBJ/lsan_off.o"

# build_harness <name> "<defines>" <extra sources under src/main or src/test/unit...>
# The harness is C and #includes the receiver under test (so it can reset that receiver's file-scope
# state per input); the extra sources are linked as separate objects.
build_harness() {
    local name=$1 defs=$2; shift 2
    local d=$OBJ/$name defargs=() s
    rm -rf "$d"; mkdir -p "$d"
    for s in $defs; do defargs+=(-D "$s"); done
    local objs=() i=0 f o
    for s in "$@"; do
        if [ -f "$MAIN/$s" ]; then f=$MAIN/$s; else f=$UNIT/$s; fi
        o=$d/$(echo "$s" | tr '/' '_').o
        objs+=("$o")
        $CC "${CFLAGS[@]}" "${defargs[@]}" -c "$f" -o "$o" &
        i=$((i+1))
        if [ $((i % MAYHEM_JOBS)) -eq 0 ]; then wait -n; fi
    done
    $CC "${CFLAGS[@]}" "${defargs[@]}" -I"$HARN" -c "$HARN/$name.c" -o "$d/harness.o" &
    wait
    # shellcheck disable=SC2086
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -Wl,-T,"$UNIT/pg.ld" \
        "$d/harness.o" "${objs[@]}" "$OBJ/lsan_off.o" -lm -o "$OUT/$name"
    # shellcheck disable=SC2086
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -Wl,-T,"$UNIT/pg.ld" \
        "$d/harness.o" "${objs[@]}" "$OBJ/standalone_main.o" "$OBJ/lsan_off.o" -lm -o "$OUT/$name-standalone"
}

build_harness fuzz_rx_sumd "USE_SERIALRX_SUMD=" common/crc.c common/streambuf.c
build_harness fuzz_rx_xbus "USE_SERIALRX_XBUS=" common/crc.c common/streambuf.c
build_harness fuzz_rx_ibus "USE_SERIALRX_IBUS=" telemetry/ibus_shared.c
build_harness fuzz_msp_serial "USE_CLI= USE_MSP_DISPLAYPORT= USE_RANGEFINDER_MT= USE_OPTICALFLOW_MT=" common/crc.c common/streambuf.c
build_harness fuzz_uvarint "" common/uvarint.c
build_harness fuzz_huffman "USE_HUFFMAN=" common/huffman.c common/huffman_table.c
build_harness fuzz_typeconversion "" common/typeconversion.c

# The firmware's own gtest unit tests for the same code, built with the project's normal flags (no
# sanitizers) into obj/test, where mayhem/test.sh runs them. The mavlink submodule is a Makefile
# prerequisite that would need the network; none of these tests include its headers, so it is
# satisfied with an empty stand-in directory.
UNIT_TESTS=(
    rx_sumd_unittest rx_xbus_unittest rx_ibus_unittest rx_crsf_unittest msp_serial_unittest
    encoding_unittest huffman_unittest uvarint_unittest telemetry_ibus_unittest
    blackbox_encoding_unittest gps_conversion_unittest
)
MAVLINK_STUB=$SRC/obj/mavlink-stub
mkdir -p "$MAVLINK_STUB"
: > "$MAVLINK_STUB/.git"
rm -rf "$SRC/obj/test"
UNIT_GOALS=()
for t in "${UNIT_TESTS[@]}"; do UNIT_GOALS+=("../../obj/test/$t/$t"); done
make -C "$SRC/src/test" -j"$MAYHEM_JOBS" CC="$CC" CXX="$CXX" MAVLINK_LIB_DIR="$MAVLINK_STUB" "${UNIT_GOALS[@]}"
