# todext

A minimal zero-dependency command-line tool that scans source code and extracts developer annotation comments — `TODO`, `BUG`, and `NOTE` tags — from any text-based file.

## Features

- Scans a file or directory tree for `TODO`, `BUG`, and `NOTE` tags (whole-word, case-insensitive)
- Filters out false positives by verifying matches are actual comment lines (`//`, `#`, `--`, `*`, `;`)
- Captures multiline comment continuations as a single entry
- Color-coded terminal output grouped by file
- Markdown export with a per-file summary table
- Editor integration: jump directly to any annotation with `-o FILE:LINE`
- Uses `ripgrep` if available on `PATH`, falls back to `grep`

## Requirements

- C++17-capable compiler (`g++`)
- GNU Make
- `rg` (ripgrep) — optional but recommended; `grep` is the fallback

## Build

```sh
make
```

The compiled binary is placed at `build/todext`.

## Install

Edit the `PREFIX` variable in the `Makefile` to your preferred bin directory, then run:

```sh
make install
```

## Usage

```
todext [OPTIONS] [PATH]
```

`PATH` can be a file or directory (defaults to the current directory).

| Option | Description |
|--------|-------------|
| `-e`, `--export [FILE]` | Export results to Markdown (default: `todo.md`) |
| `-o FILE:LINE` | Open a specific entry in an editor |
| `-E EDITOR` | Override the editor used by `-o` (default: `$EDITOR`) |
| `--no-color` | Disable ANSI color output |
| `-v`, `--version` | Print version and copyright |
| `-h`, `--help` | Print usage |

### Examples

```sh
# Scan the current directory
todext .

# Scan a specific directory
todext src/

# Scan and export results to todo.md
todext . -e

# Export to a named file
todext . -e myreport.md

# Plain output (no color)
todext . --no-color

# Open an annotation in $EDITOR
todext -o src/main.cpp:42

# Open in VS Code
todext -o src/main.cpp:42 -E code
```

## Output

Terminal output is grouped by file, with each entry showing the tag type, line number, and comment text. The `-e` flag produces a Markdown file with a summary table listing TODO/BUG/NOTE counts per file followed by the full annotation list.

## License

MIT License — see [LICENSE](LICENSE) for details.
