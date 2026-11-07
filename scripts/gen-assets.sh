#!/usr/bin/env bash
# Packs game data (TC/openliero) into game/assets-gen for the APK, plus a
# manifest (first line = content hash, used to re-install on updates).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/game/assets-gen"
rm -rf "$OUT"
mkdir -p "$OUT"
cp -r "$ROOT/game/TC" "$OUT/TC"
cp -r "$ROOT/game/ui" "$OUT/ui"
cd "$OUT"
FILES=$(find TC ui -type f | LC_ALL=C sort)
HASH=$(cat $FILES | sha256sum | cut -c1-16)
{ echo "$HASH"; echo "$FILES"; } > liero_files.txt
echo "assets: $(echo "$FILES" | wc -l) files, version $HASH"
