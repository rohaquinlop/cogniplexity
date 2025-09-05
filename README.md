# Cognity

Minimal, fast cognitive complexity for multiple languages using Tree‑sitter.

Cognity parses source code with Tree‑sitter, builds a General Syntax Graph (GSG) of control‑flow constructs, and computes cognitive complexity from that graph. The GSG keeps the computation consistent across languages, while small per‑language builders focus only on AST→GSG mapping.

## Highlights
- Multi‑language: Python, JavaScript, TypeScript (incl. TSX), C, C++.
- Accurate control‑flow: if/elif/else, loops, switch/case, ternary, try/except (Py), with (Py).
- Boolean alternation costs and nested function handling (e.g., C++ lambdas).
- Standalone binary with no runtime deps beyond Tree‑sitter.

## Build

Requirements
- CMake ≥ 3.16
- A C++20+ compiler (C++23 is enabled)
- Internet if fetching grammars (default)

Quick build (fetch grammars)
- `cmake -S . -B build`
- `cmake --build build -j`

Offline build (no network)
- `cmake -S . -B build -DCOGNITY_FETCH_GRAMMARS=OFF`
- `cmake --build build -j`

CMake options
- `-DCOGNITY_FETCH_GRAMMARS=ON|OFF` — fetch Tree‑sitter grammars (default: ON)
- `-DCMAKE_BUILD_TYPE=Debug|Release` — standard CMake build type

Scripted build
- `bash scripts/build.sh` — simple wrapper around CMake
  - `--fetch on|off` (default: on)
  - `--type Debug|Release` (default: Release)
  - `-b, --build-dir <dir>` (default: build)
  - `-j <N>` parallel jobs
  - `--clean` reconfigure from scratch

## Usage

Run the CLI with one or more file paths:
- `./build/cognity src/file.py src/file.cpp src/file.ts`

Output prints each function and its cognitive complexity. Language is detected by file extension.

CLI options
- `-mx, --max-complexity <int>` — max allowed cognitive complexity (default 15)
- `-q, --quiet` — quiet mode
- `-i, --ignore-complexity` — do not fail based on max
- `-d, --detail <low|normal>` — detail level (default normal)
- `-s, --sort <asc|desc|name>` — sort order (default name)
- `-csv, --output-csv` — CSV output
- `-json, --output-json` — JSON output

## Internals
- General Syntax Graph (GSG): a normalized tree of Function, If/ElseIf/Else, For/While/DoWhile, Switch/Case, Ternary, Try/Except/With (Py), and Expr cost nodes.
- Builders: small, per‑language mappers from Tree‑sitter ASTs to the GSG.
- Calculator: a single pass that evaluates cognitive complexity over the GSG.

## Troubleshooting
- Fetching grammars fails: pass `-DCOGNITY_FETCH_GRAMMARS=OFF` and rely on vendored grammars (Python is vendored; others require network).
- CMake < 3.16: upgrade CMake.
