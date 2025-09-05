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

Run the CLI with one or more paths (files or directories):
- `./build/cognity src/file.py src/file.cpp src/file.ts` — files
- `./build/cognity src/` — recursively scans directory for supported files

Output prints each function and its cognitive complexity. Language is detected by file extension.

CLI options
- `-mx, --max-complexity <int>` — max allowed cognitive complexity (default 15)
- `-q, --quiet` — quiet mode
- `-i, --ignore-complexity` — do not fail based on max
- `-d, --detail <low|normal>` — detail level (default normal)
- `-s, --sort <asc|desc|name>` — sort order (default name)
- `-csv, --output-csv` — CSV output (Function printed as `name@line`; `line` also included as a separate column)
- `-json, --output-json` — JSON output (`function` is `name@line`; `line` remains as a separate field)
- `-l, --lang <list>` — comma‑separated languages to include when scanning directories or files. Examples: `-l py,js`, `-l typescript`, `-l c,cpp`. Supported: `py|python`, `js|javascript`, `ts|typescript|tsx`, `c`, `cpp|c++|cc|cxx`.
- `-fw, --max-fn-width <int>` — truncate the printed Function column to at most this width. When truncation occurs and width > 3, an ellipsis (`...`) is appended; the `@line` suffix is preserved when possible. If omitted or set to 0, no truncation is applied.

Help
- `-h, --help` — print a short usage summary and exit.

### Language Filtering
- Purpose: restrict scanning to specific languages when passing files or directories.
- Format: `-l` or `--lang` followed by a comma‑separated list; the flag can be repeated.
- Supported tokens: `py|python`, `js|javascript`, `ts|typescript|tsx`, `c`, `cpp|c++|cc|cxx`.
- Default: if `--lang` is not provided, all supported languages are included.

Examples
- `./build/cognity src -l py,js` — only Python and JavaScript files in `src`.
- `./build/cognity src lib -l c,cpp` — scan both paths, only C/C++ files.
- `./build/cognity project -l typescript` — only TypeScript/TSX files.

Notes
- Unsupported files are skipped silently.
- If no files match the filters, the tool prints: `No matching source files found`.

## Internals
- General Syntax Graph (GSG): a normalized tree of Function, If/ElseIf/Else, For/While/DoWhile, Switch/Case, Ternary, Try/Except/With (Py), and Expr cost nodes.
- Builders: small, per‑language mappers from Tree‑sitter ASTs to the GSG.
- Calculator: a single pass that evaluates cognitive complexity over the GSG.

## Supported Extensions

| Language    | Extensions                 | Status       |
|-------------|----------------------------|--------------|
| Python      | .py                        | Supported    |
| JavaScript  | .js, .mjs, .cjs            | Supported    |
| TypeScript  | .ts, .tsx                  | Supported    |
| C           | .c                         | Supported    |
| C++         | .cpp, .cc, .cxx            | Supported    |
| Java        | .java                      | Detected only (not computed) |

Note: Java files are detected and skipped (no builder yet).

## Example Output

```
$ ./build/cognity src -l py,cpp
src/parser.py
  Function           cognitive complexity
  parse_items@12     4
  parse_block@58     7

src/utils.cpp
  Function           cognitive complexity
  utils::format@23   2
```

## Troubleshooting
- Fetching grammars fails: pass `-DCOGNITY_FETCH_GRAMMARS=OFF` and rely on vendored grammars (Python is vendored; others require network).
- CMake < 3.16: upgrade CMake.
