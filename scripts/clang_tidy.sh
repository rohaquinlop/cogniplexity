#!/usr/bin/env bash
set -euo pipefail

# Run clang-tidy across project sources and optionally check formatting.
# Environment variables to customize:
#   BUILD_DIR       - CMake build directory (default: build)
#   CLANG_TIDY_BIN  - clang-tidy executable (default: clang-tidy)
#   CLANG_FORMAT_BIN- clang-format executable (default: clang-format)
#   CHECKS          - clang-tidy checks override (default: use .clang-tidy if present)
#   HEADER_FILTER   - regex to limit headers (default: ^(src|include)/)
#   FIX             - 1 to apply fixes (default: 0)
#   JOBS            - parallel jobs for analysis (default: CPU count)
#   FORMAT_CHECK    - 1 to run clang-format --dry-run --Werror when .clang-format exists (default: 1)

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR=${BUILD_DIR:-build}
CLANG_TIDY_BIN=${CLANG_TIDY_BIN:-clang-tidy}
CLANG_FORMAT_BIN=${CLANG_FORMAT_BIN:-clang-format}
HEADER_FILTER=${HEADER_FILTER:-'^(src|include)/'}
FIX=${FIX:-0}
FORMAT_CHECK=${FORMAT_CHECK:-1}

# Determine parallelism
if command -v sysctl >/dev/null 2>&1; then
  JOBS_DEFAULT=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
elif command -v nproc >/dev/null 2>&1; then
  JOBS_DEFAULT=$(nproc)
else
  JOBS_DEFAULT=4
fi
JOBS=${JOBS:-$JOBS_DEFAULT}

echo "[clang-tidy] Using build dir: $BUILD_DIR"

# Ensure compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "[clang-tidy] Generating compile_commands.json via CMake..."
  cmake -S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
fi

# Prefer compile_commands from build dir
TIDY_PROJECT_PATH="$BUILD_DIR"

# Collect files to analyze
if command -v git >/dev/null 2>&1; then
  FILE_LIST_CMD="git ls-files | grep -E '^(src|include)/.*\\.(c|cc|cpp|cxx)$' || true"
else
  FILE_LIST_CMD="find src include \\(-name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx'\\) 2>/dev/null || true"
fi

FILE_COUNT=$(eval "$FILE_LIST_CMD" | wc -l | tr -d ' ')
if [ "$FILE_COUNT" = "0" ]; then
  echo "[clang-tidy] No source files found under src/ or include/." >&2
  exit 0
fi

# Build clang-tidy args
TIDY_ARGS=("-p" "$TIDY_PROJECT_PATH" "--header-filter=$HEADER_FILTER" "-warnings-as-errors=*")
if [ -n "${CHECKS:-}" ]; then
  TIDY_ARGS+=("--checks=$CHECKS")
fi
if [ "$FIX" = "1" ]; then
  TIDY_ARGS+=("-fix")
fi

echo "[clang-tidy] Analyzing ${FILE_COUNT} file(s) with $JOBS job(s)..."
eval "$FILE_LIST_CMD" | xargs -P "$JOBS" -n 1 "$CLANG_TIDY_BIN" "${TIDY_ARGS[@]}"

# Optional formatting check if a .clang-format is present
if [ "$FORMAT_CHECK" = "1" ]; then
  if [ -f ".clang-format" ] && command -v "$CLANG_FORMAT_BIN" >/dev/null 2>&1; then
    echo "[clang-format] Checking formatting (dry-run, Werror)..."
    if command -v git >/dev/null 2>&1; then
      eval "$FILE_LIST_CMD" | tr '\n' '\0' | xargs -0 -P "$JOBS" -n 50 "$CLANG_FORMAT_BIN" --dry-run --Werror --style=Google
    else
      find src include \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) -print0 2>/dev/null | \
        xargs -0 -P "$JOBS" -n 50 "$CLANG_FORMAT_BIN" --dry-run --Werror --style=Google
    fi
  else
    echo "[clang-format] Skipping (no .clang-format or clang-format not found)."
  fi
fi

echo "[clang-tidy] Completed successfully."
