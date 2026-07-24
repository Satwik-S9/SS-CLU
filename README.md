# SS-CLU — Sri's Simple Command Line Utilities

A collection of small, self-contained command-line utilities. Each tool lives in its own subdirectory with its own build system and license.

---

## Tools

| Tool | Version | Language | Description |
|------|---------|----------|-------------|
| [csview](#csview) | 1.1.0 | C (C11) | CSV pretty-printer with Unicode table borders |
| [mdview](#mdview) | 1.0.2 | C (C11) | Minimal Markdown renderer for the terminal |
| [basic-editor](#basic-editor) | 1.0 | C (C99) | A basic text editor for making quick edits |
| [todext](#todext) | 1.2 | C++ (C++17) | Source-code annotation extractor (TODO/BUG/NOTE) |

---

## csview

Renders CSV files as formatted tables in the terminal using Unicode box-drawing characters.

**Features:**
- Rounded-corner box-drawing table borders (`╭`, `╰`, `┬`, `┼`, …)
- Per-column ANSI colors with bold white headers
- `--head N` / `--tail N` to preview the first or last N rows
- Auto-detects terminal width; collapses middle columns with `...` when too narrow
- Wraps long column headers onto two lines to minimize column width
- Full RFC 4180 CSV parsing: quoted fields, embedded commas, `""` escapes, CRLF/LF
- Reads from a file argument or stdin

**Build and install:**
```sh
cd csview
make
make install   # installs to ~/.local/bin (override with PREFIX=...)
```

**Usage:**
```
csview [OPTIONS] [FILE]

Options:
  --head N    Show only the first N rows
  --tail N    Show only the last N rows
  --no-color  Disable ANSI color output
  -h, --help  Show help message
```

**Example:**
```sh
csview data.csv
cat data.csv | csview --head 10
```

**License:** [MIT](csview/LICENSE)

---

## mdview

A minimal Markdown renderer that displays `.md` files in the terminal with ANSI color and style formatting.

**Supported elements:**

| Element | Rendering |
|---------|-----------|
| H1 | Bold red with `[1]` prefix |
| H2 | Cyan with `{2}` prefix |
| H3 | Yellow with `(3)` prefix |
| H4+ | Magenta/gray with `(N)` prefix |
| Unordered lists (`-`, `*`) | Colored `•` bullet |
| Blockquotes (`>`) | Gray `│` prefix |
| Fenced code blocks | Divider line |
| `**bold**` | ANSI bold |
| `*italic*` | ANSI italic |
| `` `code` `` | Reverse video |

**Build and install:**
```sh
cd mdview
make
make install   # installs to ~/.local/bin (override with PREFIX=...)
```

**Usage:**
```sh
mdview FILE.md
```

**Platform note:** Requires a POSIX environment (`_GNU_SOURCE`/`getline`). Does not build natively on Windows without WSL or a POSIX layer.

**License:** [MIT](mdview/LICENSE)

---

## basic-editor
**This app is currently in development**

## todext

Scans source files and directory trees for developer annotations tagged `TODO`, `BUG`, or `NOTE`, and displays them with ANSI color coding grouped by file.

**Features:**
- Uses `ripgrep` (`rg`) when available, falls back to `grep`
- Filters to actual comment lines only (`//`, `#`, `--`, `*`, `;`) — ignores annotations in string literals or identifiers
- Captures multiline comment continuations as a single entry
- Color-coded output: TODOs in yellow, BUGs in red, NOTEs in cyan
- Markdown export (`-e`) with per-file summary and a count table
- Editor integration (`-o FILE:LINE`) with special handling for VS Code, Sublime Text, and generic editors

**Requirements:** C++17 compiler, `std::filesystem` support. Optionally: `rg` (ripgrep) for faster scanning.

**Build and install:**
```sh
cd todext
make
make install
```

**Usage:**
```
todext [OPTIONS] [PATH...]

Options:
  -e, --export FILE   Export annotations to a Markdown file
  -o FILE:LINE        Open annotation in $EDITOR
  -h, --help          Show help message
```

**Example:**
```sh
todext src/
todext --export notes.md .
```

**License:** [MIT](todext/LICENSE)

---

## Building all tools

Each tool is built independently with GNU Make:

```sh
for d in csview mdview todext; do
  (cd "$d" && make)
done
```

---

## License

Each tool is licensed individually under its own MIT license. See the `LICENSE` file in each tool's subdirectory.
