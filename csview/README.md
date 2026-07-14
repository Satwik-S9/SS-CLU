# csview

A minimal zero-dependency command-line CSV pretty-printer that renders tabular data as a formatted table in your terminal using Unicode box-drawing characters.

```
╭──────────────────┬──────┬────────────╮
│ Name             │ Age  │ City       │
├──────────────────┼──────┼────────────┤
│ Alice            │ 30   │ New York   │
│ Bob              │ 25   │ London     │
│ Carol            │ 35   │ Tokyo      │
╰──────────────────┴──────┴────────────╯
```

## Features

- Renders CSV files as neatly bordered tables with rounded corners
- Optional ANSI color output — each column gets a distinct color, headers are bold white
- `--head` / `--tail` modes to preview the first or last N rows
- Auto-detects terminal width and collapses middle columns with a `...` placeholder when the table is too wide to fit
- Wraps long column headers onto two lines to keep column widths compact
- Handles RFC 4180-compliant CSV: quoted fields, embedded commas, escaped double-quotes (`""`), CRLF and LF line endings
- Reads from a file or from stdin (pipe-friendly)
- Zero external dependencies — only the C standard library and POSIX

## Installation

**Requirements:** GCC, GNU Make, a POSIX-compatible system (Linux, macOS, WSL)

```sh
# Clone or download the source, then:
make
make install              # installs to ~/.local/bin by default
```

To install to a different location:

```sh
make install PREFIX=/usr/local/bin
```

To uninstall:

```sh
make uninstall
# or: make uninstall PREFIX=/usr/local/bin
```

## Usage

```
csview [OPTIONS] [FILE]
cat file.csv | csview [OPTIONS]
```

### Options

| Flag | Description |
|---|---|
| `-c`, `--color` | Colorize each column with ANSI colors |
| `-H`, `--head [N]` | Show the first N data rows (default: 5) |
| `-T`, `--tail [N]` | Show the last N data rows (default: 5) |
| `--no-header` | Treat the first row as data instead of a header |
| `-h`, `--help` | Show help and exit |
| `-v`, `--version` | Show version and copyright info |

### Examples

```sh
# Print all rows as a plain table
csview data.csv

# Colorized output, show first 10 rows
csview --color --head 10 data.csv

# Show last 5 rows (default N)
csview --tail data.csv

# Show last 20 rows
csview --tail 20 data.csv

# Pipe from stdin with color
cat data.csv | csview --color

# No header row
csview --no-header data.csv
```

When `--head` or `--tail` is active, a summary is printed to stderr:

```
[head 10]  showing 10 of 250 data row(s)
```

When the table is too wide for the terminal, middle columns are collapsed:

```
[csview] table too wide for terminal (80 cols); showing 5 of 12 columns
```

## Building from Source

```sh
make        # build to build/csview
make clean  # remove build artifacts
```

Compiler flags: `-Wall -Wextra -pedantic -O2 -std=c11`

## License

MIT License — see [LICENSE](LICENSE) for details.
