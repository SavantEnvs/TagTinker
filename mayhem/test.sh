#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer oracle for the protocol/ and nfc/ libraries. RUNS /mayhem/tagtinker_kat
# (built by mayhem/build.sh with normal flags) and compares its output with values derived independently
# of the C code (CRC, frame layouts, RLE bit packing and the NDEF/ESL id encoding, worked out by hand).
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


KAT="${KAT:-$SRC/tagtinker_kat}"
if [ ! -x "$KAT" ]; then echo "FATAL: $KAT missing (mayhem/build.sh must build it)" >&2; emit_ctrf "tagtinker-kat" 0 1 0; exit 1; fi

passed=0; failed=0
check() { # check <name> <expected> <actual>
  if [ "$2" == "$3" ]; then passed=$((passed+1)); else failed=$((failed+1)); echo "FAIL $1" >&2; echo "  expected: $2" >&2; echo "  actual:   $3" >&2; fi
}
run() { "$KAT" "$@" 2>&1 || true; }

BC=A4012345678913172
PLID=d5ddd204

check crc16 "crc=837f" "$(run crc 313233343536373839)"

check barcode-hd-s "$(printf 'plid=%s\ntype=1317\nprofile=152x152 kind=1 color=0 model=SmartTag HD S' $PLID)" "$(run barcode $BC)"
check barcode-dm110 "$(printf 'plid=%s\ntype=1275\nprofile=320x192 kind=1 color=0 model=DM110' $PLID)" "$(run barcode A4012345678912752)"
check barcode-unknown-type "$(printf 'plid=%s\ntype=9999\nprofile=unknown' $PLID)" "$(run barcode A4012345678999992)"
check barcode-wrong-length "invalid" "$(run barcode short)"

frames="$(run frames $BC)"
expect() { check "frame-$1" "$1=$2" "$(printf '%s\n' "$frames" | grep "^$1=")"; }
expect ping    85d5ddd2049701000000010101010101010101010101010101010101010174e8
expect wake    85d5ddd2041701000000010101010101010101010101010101010101010101011b92
expect refresh 85d5ddd2043400000001000000000000000000000000000000000000427d
expect param   85d5ddd2043400000005014000020200980128000800100000880000000000005cfd
expect data    85d5ddd20434000000200102000102030405060708090a0b0c0d0e0f101112138e0f
expect addr    85d5ddd20406aabb64f8
expect bpage   85000000000699000012341291
expect bdebug  850000000006f10000000a5d14

PIX=$(printf '0%.0s' $(seq 200); printf '1%.0s' $(seq 100); printf '000'; printf '1%.0s' $(seq 50))
check payload-auto "$(printf 'bytes=20 comp=2\ndata=00c8032306400000000000000000000000000000')" "$(run payload auto $PIX)"
check payload-rle  "$(printf 'bytes=20 comp=2\ndata=00c8032306400000000000000000000000000000')" "$(run payload rle $PIX)"
check payload-raw  "$(printf 'bytes=60 comp=0\ndata=00000000000000000000000000000000000000000000000000fffffffffffffffffffffffff1ffffffffffff80000000000000000000000000000000')" "$(run payload raw $PIX)"
ALT=$(printf '01%.0s' $(seq 100))
check payload-auto-keeps-raw "bytes=40 comp=0" "$(run payload auto $ALT | head -1)"

check nfc-decoded "result=1 barcode=A4012345678913176" "$(run nfc 041122334455667700000000e1101200031dd1011955046578616d706c652e636f6d2f702f316543663959744b424dfe)"
check nfc-imagotag-link "result=2 barcode=" "$(run nfc 041122334455667700000000e11012000319d1011555046e66632e696d61676f7461672e636f6d2f616263fe)"
check nfc-www-prefix-not-imagotag "result=3 barcode=" "$(run nfc 041122334455667700000000e11012000319d1011555026e66632e696d61676f7461672e636f6d2f616263fe)"
check nfc-other-host "result=3 barcode=" "$(run nfc 041122334455667700000000e11012000314d1011055046578616d706c652e636f6d2f616263fe00)"
check nfc-no-capability-container "result=3 barcode=" "$(run nfc 04112233445566770000000000101200031dd1011955046578616d706c652e636f6d2f702f316543663959744b424dfe)"
check nfc-unreadable "result=0 barcode=" "$(run nfc "")"
check nfc-too-short "result=3 barcode=" "$(run nfc 04112233445566770000000000)"
check host-match "host=1" "$(run host HTTPS://NFC.imagotag.com:8080/x nfc.imagotag.com)"
check host-suffix-attack "host=0" "$(run host nfc.imagotag.com.evil.com/x nfc.imagotag.com)"

emit_ctrf "tagtinker-kat" "$passed" "$failed" 0
