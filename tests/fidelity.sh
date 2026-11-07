#!/usr/bin/env bash
# Runs the deterministic simulation scenarios with two harness binaries
# (reference = pristine upstream OpenLiero, candidate = this port) and fails if
# any frame of any trace differs.
#   tests/fidelity.sh <reference sim> <candidate sim> <TC dir> [runner prefix for candidate]
set -euo pipefail
REF="$1"; CAND="$2"; TC="$3"; RUN="${4:-}"
OUT="${OUT_DIR:-$(mktemp -d)}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
fail=0; n=0
for spec in "cpu 1 20000" "cpu 7 20000" "cpu 1234 20000" "scripted 42 12000" "scripted 99 12000" \
            "scripted 2024 12000" "rope 3 8000" "rope 77 8000"; do
  set -- $spec
  "$REF" "$TC" "$1" "$2" "$3" "$OUT/ref_$1_$2.txt" >/dev/null
  $RUN "$CAND" "$TC" "$1" "$2" "$3" "$OUT/cand_$1_$2.txt" >/dev/null
  n=$((n+1))
  frames=$(grep -c '^[0-9]' "$OUT/ref_$1_$2.txt")
  if cmp -s "$OUT/ref_$1_$2.txt" "$OUT/cand_$1_$2.txt"; then
    echo "PASS  $1 seed=$2  ($frames frames identical; $(tail -1 "$OUT/ref_$1_$2.txt"))"
  else
    diff "$OUT/ref_$1_$2.txt" "$OUT/cand_$1_$2.txt" > "$OUT/diff_$1_$2.txt" || true
    echo "FAIL  $1 seed=$2  first difference:"
    head -6 "$OUT/diff_$1_$2.txt" || true
    fail=1
  fi
done
echo "$n scenarios checked"
exit $fail
