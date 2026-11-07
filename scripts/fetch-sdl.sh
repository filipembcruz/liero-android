#!/usr/bin/env bash
# Fetches the SDL2 source used for the Android build (C library + Java glue).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/android/app/jni/SDL"
SDL_TAG="${SDL_TAG:-release-2.30.12}"
if [ -f "$DEST/CMakeLists.txt" ]; then echo "SDL already present"; exit 0; fi
git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL "$DEST"
