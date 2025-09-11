# Cognity

Minimal, fast cognitive complexity across languages — powered by Tree‑sitter.

> Cognitive complexity measures how hard code is to understand.

Use Cognity to scan code, list function‑level complexity, and keep reviews focused on what’s hardest to read.

— [Installation](#installation) • [Quick Start](#quick-start) • [Configuration](#configuration) • [Build](#build)

## Installation

- From source (CMake ≥ 3.16, C++20+):
  - `cmake -S . -B build`
  - `cmake --build build -j`
  - The binary is at `build/cognity`

## Quick Start

Analyze files and directories; languages are inferred by extension.

```bash
# Analyze a project
cognity src/

# Analyze specific files
cognity file.py file.cpp

# Filter languages
cognity src -l py,js

# Set a threshold and output JSON/CSV
cognity . -mx 10 --output-json --output-csv

# Quiet mode (no output, exit code only)
cognity . -mx 10 -q

# See all options
cognity --help
```

Example text output
```bash
src/parser.py
  Function           cognitive complexity
  parse_items@12     4
```

## Configuration

Place a `cognity.toml` in your project to set defaults (CLI flags always win).

```toml
# cognity.toml
paths = ["src", "include"]
exclude = ["node_modules", "dist"]
max_complexity = 15
detail = "normal" # or "low"
languages = ["py", "js", "ts", "c", "cpp"]
output_json = false
output_csv = false
```

## Supported Languages

- Python (`.py`)
- JavaScript (`.js`, `.mjs`, `.cjs`)
- TypeScript/TSX (`.ts`, `.tsx`)
- C (`.c`), C++ (`.cpp`, `.cc`, `.cxx`)

Unsupported files are skipped. If filters match nothing, Cognity reports: `No matching source files found`.

Note on quiet mode: with `-q/--quiet`, Cognity prints nothing and returns only an exit code (0 if no function exceeds `-mx`, 2 if any do, 1 on error).

## Build

Requirements
- CMake ≥ 3.16
- C++20+ compiler (C++23 enabled)

Build
- `cmake -S . -B build`
- `cmake --build build -j`
- Or run `bash scripts/build.sh`
