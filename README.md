# Cognity

Minimal, fast cognitive complexity across languages — powered by Tree‑sitter.

Cognity parses your source code with Tree‑sitter, builds a small General Syntax Graph (GSG) of control‑flow constructs, and computes cognitive complexity consistently across languages. Per‑language builders only map AST → GSG; the calculation is shared.

## Quick Start

- `cognity <path...>` — pass files and/or directories; directories are scanned recursively.
- Prints each function with its cognitive complexity; language is inferred from file extension.

Examples
- `cognity src/` — scan a project
- `cognity file.py file.cpp` — scan specific files
- `cognity src -l py,js` — filter to Python and JavaScript

Example output
```
$ cognity src -l py,cpp
src/parser.py
  Function           cognitive complexity
  parse_items@12     4

src/utils.cpp
  Function           cognitive complexity
  utils::format@23   2
```

## Install

- Prebuilt binaries: download from GitHub Releases (https://github.com/rohaquinlop/cognity/releases) and place `cognity` on your `PATH` (coming soon).
- Homebrew: `brew install rohaquinlop/tap/cognity` (planned).
- From source: see Build below.

Supported languages
- Python (`.py`)
- JavaScript (`.js`, `.mjs`, `.cjs`)
- TypeScript/TSX (`.ts`, `.tsx`)
- C (`.c`), C++ (`.cpp`, `.cc`, `.cxx`)
- Java (`.java`) is detected and skipped (no complexity yet)

## Usage

Basic form
- `cognity <paths...> [options]`

Key options
- `-l,  --lang <list>`: language filter, comma‑separated (e.g. `py,js`, `c,cpp`).
- `-mx, --max-complexity <n>`: threshold (default 15).
- `-q,  --quiet`: print only offenders (unless `-i`).
- `-i,  --ignore-complexity`: do not enforce the threshold.
- `-s,  --sort <asc|desc|name>`: sort order (default name).
  - `asc`/`desc`: globally sort functions across all files by complexity (text, JSON, CSV).
  - `name`: sort by file name (then function name). Text, JSON, and CSV outputs print a single global table sorted by file and function, same layout as `asc/desc`.
- `-d,  --detail <low|normal>`: output detail (default normal). In `low` detail, only functions exceeding the threshold are shown (unless `-i`).
- `-csv, --output-csv`: CSV output.
- `-json, --output-json`: JSON output.
- `-x,  --exclude <list>`: comma‑separated files or directories to exclude.
- `-fw, --max-fn-width <n>`: truncate printed function column to at most `n`.
- `-h,  --help`: show help and exit.
 - `--version`: show version and exit.

Notes
- Unsupported files are skipped.
- If filters match nothing, you’ll see: `No matching source files found`.
- Directory scans respect `.gitignore` files (ignored files and folders are skipped).
- `--exclude` paths are matched literally (no globs). Files are compared by path; directories exclude everything under them. Relative paths resolve from the current working directory.

## Config (cognity.toml)

You can place a `cognity.toml` file in your working directory to provide defaults for the same options as the CLI. Any CLI option overrides the config file.

Supported keys (top‑level):
- `paths`: array or comma‑separated string, e.g. `["src", "lib"]` or "src,lib".
- `exclude` (or `excludes`): array or comma‑separated string of files/dirs to skip.
- `max_complexity` or `max_complexity_allowed`: integer.
- `quiet`: boolean.
- `ignore_complexity`: boolean.
- `detail`: "low" | "normal".
- `sort`: "asc" | "desc" | "name".
- `output_csv`: boolean.
- `output_json`: boolean.
- `max_fn_width` or `max_function_width`: integer.
- `lang` or `languages`: array or comma‑separated string, e.g. `["py", "js"]` or "py,js".

Example `cognity.toml`:
```
# Default scan targets
paths = ["src", "include"]

# Exclude generated/vendor code
exclude = ["node_modules", "dist", "src/legacy/old_impl.cpp"]

# Output and formatting
detail = "normal"
sort = "name"
quiet = false
output_json = false
output_csv = false
max_fn_width = 40

# Threshold behavior
max_complexity = 15
ignore_complexity = false

# Language filter
languages = ["py", "js", "ts", "c", "cpp"]
```

If neither CLI nor `cognity.toml` provides any `paths`, Cognity errors with: "expected at least one path".

## How it works

- General Syntax Graph (GSG): normalized nodes for functions, if/else, loops, switch/case, ternary, try/except/with (Py), and boolean‑cost expressions.
- Per‑language builders: small AST→GSG mappers for Python, JS/TS/TSX, C/C++.
- Single complexity pass over the GSG for consistent scoring.

## Build

Requirements
- CMake ≥ 3.16
- C++20+ compiler (C++23 enabled)
- Network access (grammars are always fetched)

Quick build
- `cmake -S . -B build`
- `cmake --build build -j`

Scripted build
- `bash scripts/build.sh`
  - `--type Debug|Release` (default Release)
  - `-b, --build-dir <dir>` (default `build`)
  - `-j <N>` parallel jobs
  - `--clean` reconfigure from scratch

Troubleshooting
- If grammar downloads fail, check network access or GitHub availability.
- CMake too old: upgrade to ≥ 3.16.

Pinned grammar versions
- The build fetches specific tagged revisions of each grammar to keep results stable.
- Tags are defined in `CMakeLists.txt` (variables `TS_*_TAG`). Update them there to change versions.
