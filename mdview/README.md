# mdview

A minimal, zero-dependency terminal Markdown renderer. Reads a `.md` file and prints it to the terminal with ANSI color and style formatting — making Markdown readable without leaving your shell.

**Author:** Satwik Srivastava 
**Version:** 1.0.2  
**License:** MIT

---

## Features

- Renders headings (H1–H4+) with distinct ANSI colors
- Unordered lists with colored bullet points
- Blockquotes with a vertical bar prefix
- Fenced code blocks with a divider line
- Inline **bold**, *italic*, and `code` spans
- Single C file — the entire implementation is ~205 lines
- No external dependencies; only the C standard library

## Supported Markdown

| Element | Syntax | Rendering |
|---|---|---|
| Heading 1 | `# Title` | Bold red, prefixed `[1]` |
| Heading 2 | `## Title` | Bold cyan, prefixed `{2}` |
| Heading 3 | `### Title` | Bold yellow, prefixed `(3)` |
| Heading 4+ | `#### Title` | Bold magenta/gray, prefixed `(N)` |
| Unordered list | `- item` or `* item` | Blue `•` bullet |
| Blockquote | `> text` | Gray `│ ` prefix |
| Fenced code block | ` ``` ` | Divider line |
| Bold | `**text**` | ANSI bold |
| Italic | `*text*` | ANSI italic |
| Inline code | `` `code` `` | Reverse video |

> **Note:** This targets a practical subset of CommonMark, not a full implementation. Ordered lists, tables, links, images, and nested styles are not supported.

## Build

**With Make (recommended):**
```bash
make
```
Output binary: `build/mdview`

**Manually with GCC:**
```bash
gcc -std=c11 -O2 -o mdview mdview.c
```

**Compiler flags used by Makefile:** `-Wall -Wextra -Werror -O2`

## Install

```bash
make install
```

Installs to `$HOME/.local/bin/mdview` by default. Override with `PREFIX`:

```bash
make install PREFIX=/usr/local
```

## Usage

```bash
mdview file.md          # render a Markdown file
mdview -v               # print version info
mdview --version        # same as -v
```

**Clean build artifacts:**
```bash
make clean
```

## Platform Notes

`mdview` uses `getline()` (a POSIX extension via `_GNU_SOURCE`). It compiles and runs on Linux, macOS, and WSL. Native Windows (MSVC/MinGW without POSIX headers) is not supported without modification.

ANSI escape codes are required for styled output. Most modern terminal emulators support them.

## License

MIT License — see [LICENSE](LICENSE) for details.

