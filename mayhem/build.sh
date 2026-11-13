#!/usr/bin/env bash
#
# mayhem/build.sh — builds the fuzz harnesses (sanitized, fuzzer + standalone) and the KAT oracle probe.
#
# TagTinker is a Flipper Zero app, so its pure-logic sources (protocol/, nfc/) are compiled in place against
# mayhem/shim/, a type-only stand-in for the firmware headers. The sources themselves are not modified.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

OUT=/mayhem
B="$SRC/build/mayhem"   # `build/` is gitignored upstream
mkdir -p "$B"

SHIM=(-I"$SRC/mayhem/shim" -I"$SRC")
# Sanitized, SanCov-instrumented project objects (the fuzzed code itself carries coverage + ASan/UBSan).
FUZZ_CFLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 "${SHIM[@]}")

PROJECT_SRCS=(protocol/tagtinker_proto.c nfc/tagtinker_nfc.c)
for s in "${PROJECT_SRCS[@]}"; do
  $CC "${FUZZ_CFLAGS[@]}" -c "$SRC/$s" -o "$B/$(basename "$s" .c).fuzz.o"
done
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"

for t in fuzz_proto fuzz_nfc; do
  $CC "${FUZZ_CFLAGS[@]}" -c "$SRC/mayhem/$t.c" -o "$B/$t.o"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$t.o" \
      "$B"/tagtinker_proto.fuzz.o "$B"/tagtinker_nfc.fuzz.o "$B/lsan_off.o" -o "$OUT/$t"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$B/$t.o" "$B/standalone_main.o" \
      "$B"/tagtinker_proto.fuzz.o "$B"/tagtinker_nfc.fuzz.o "$B/lsan_off.o" -o "$OUT/$t-standalone"
done

# Oracle: known-answer probe over the same sources, NORMAL flags (no sanitizer, no -gdwarf-3), dynamically linked.
$CC -O2 $COVERAGE_FLAGS "${SHIM[@]}" "$SRC/mayhem/kat_probe.c" \
    "$SRC/protocol/tagtinker_proto.c" "$SRC/nfc/tagtinker_nfc.c" -o "$OUT/tagtinker_kat"
