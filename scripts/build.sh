#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
JOBS=""
TYPE="Release"
CLEAN=false

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  -b, --build-dir DIR    Build directory (default: build)
  -j N                   Parallel build jobs (passed to cmake --build)
  --type Debug|Release   CMake build type (default: Release)
  --clean                Remove build dir and reconfigure
  -h, --help             Show this help

Notes:
  - Grammars are always fetched from upstream (no vendored copies kept).

Examples:
  $0 --fetch on --type Debug -j 8
  $0 --fetch off -b out
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -b|--build-dir) BUILD_DIR="$2"; shift 2 ;;
    -j) JOBS="-j $2"; shift 2 ;;
    --type) TYPE="$2"; shift 2 ;;
    --clean) CLEAN=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

if $CLEAN && [[ -d "$BUILD_DIR" ]]; then
  echo "[clean] removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

echo "[configure] cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$TYPE"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$TYPE"

echo "[build] cmake --build $BUILD_DIR $JOBS"
cmake --build "$BUILD_DIR" $JOBS

echo "[done] binary: $BUILD_DIR/cognity"
