/*
 * mdview — Simple Terminal Markdown Renderer
 * Author: Satwik Srivastava
 *
 * Usage:  mdview [FILE]
 *         cat file.csv | mdview
 *
 * Options:
 *     -v, --version		 Show version information
 * No external dependencies — only the C standard library.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#define VERSION "1.1.0"
#define AUTHOR "Satwik Srivastava"

/* ANSI helpers */
#define ESC "\x1b"
#define RESET ESC "[0m"
#define BOLD  ESC "[1m"
#define UNDERLINE  ESC "[4m"
#define BOLD_OFF ESC "[22m"
#define ITALIC ESC "[3m"
#define ITALIC_OFF ESC "[23m"

/* 8-bit color helper: foreground color */
static void ansi_fg_8bit(int n) {
    printf(ESC "[38;5;%dm", n);
}
static void ansi_fg_reset() {
    printf(ESC "[39m");
}

/* choose colors for heading levels and list bullets */
static int heading_color(int level) {
    switch(level) {
        case 1: return 196; /* red */
        case 2: return 33;  /* cyan */
        case 3: return 220; /* yellow */
        case 4: return 201; /* magenta */
        default: return 250; /* light gray */
    }
}
static int list_color() { return 75; } /* blue */

// Command line flags
typedef struct _Flags {
    bool expand_links;
    bool show_version;
    bool paged_output;
    bool show_usage;
    char *mdfile_path;
} Flags;

/* --- INLINE RENDERER --------------------------------------------------- */
/*
   Supports:
   - *text* → italic
   - **text** → bold
   - `code` → reverse video
*/
static void render_inline(const char *s, bool expand_links) {
    size_t i = 0;
    size_t n = strlen(s);
    int bold = 0, italic = 0, underline = 0, highlight = 0;

    while (i < n) {
        char c = s[i];

        /* Inline code: `code` */
        if (c == '`') {
            if (!highlight) { printf("\033[48;2;%d;%d;%dm", 51, 51, 51); highlight = 1; }
            else { printf("\033[0m"); highlight = 0; }
            i++;
            continue;
        }

        /* Bold/italic markers: * or ** */
        if (c == '*') {
            size_t j = i;
            while (j < n && s[j] == '*') j++;
            int stars = (int)(j - i);

            if (stars >= 2) {
                if (!bold) { printf(BOLD); bold = 1; }
                else { printf(BOLD_OFF); bold = 0; }
            } else if (stars == 1) {
                if (!italic) { printf(ITALIC); italic = 1; }
                else { printf(ITALIC_OFF); italic = 0; }
            }
            i = j;
            continue;
        }

        /* Links */
        if (c == '[') {
            const char *link_text_start = s + i + 1;
            const char *link_text_end = strchr(link_text_start, ']');
            if (link_text_end && link_text_end[1] == '(') {
                const char *link_url_start = link_text_end + 2;
                const char *link_url_end = strchr(link_url_start, ')');
                if (link_url_end) {
                    /* Render link text in blue and underlined */
                    if (!underline) { printf(UNDERLINE); underline = 1; }
                    fwrite(link_text_start, 1, link_text_end - link_text_start, stdout);
                    
                    if (expand_links) {
                        printf(RESET " →  ");
                        printf(ESC "[34m" UNDERLINE);
                        fwrite(link_url_start, 1, link_url_end - link_url_start, stdout);
                    }

                    if (underline) { printf(RESET); underline = 0; }
                    i = (size_t)(link_url_end - s) + 1; /* Move past the closing ')' */
                    continue;
                }
            }
        }

        putchar(c);
        i++;
    }

    /* safety: close any open styles */
    if (highlight) printf(RESET);
    if (bold) printf(BOLD_OFF);
    if (italic) printf(ITALIC_OFF);
    if (underline) printf(RESET);
}

/* Trim leading spaces */
static const char *skip_leading_spaces(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int in_code_block = 0;
/* --- LINE-LEVEL MARKDOWN ------------------------------------------------ */
static void process_line(const char *line, bool expand_links) {
    const char *p = skip_leading_spaces(line);

    /* Fenced code blocks: toggle on ``` and render enclosed lines raw */
    if (strncmp(p, "```", 3) == 0) {
        if (!in_code_block) {
            in_code_block = 1;
            /* Detect the language after the backticks, if any */
            const char *lang = p + 3;
            while (*lang && isspace((unsigned char)*lang)) lang++;
            if (*lang == '\0')
                printf(ESC "[90m──────── code block ────────" RESET "\n");
            else
                printf(ESC "[90m──────── code block [%s] ────────" RESET "\n", lang);
        } else {
            in_code_block = 0;
            printf(ESC "[90m──────── end code block ────────" RESET "\n");
        }
        return;
    }

    /* Inside a code block: print the line verbatim, no markdown parsing */
    if (in_code_block) {
        printf("%s\n", line);
        return;
    }

    if (*p == '\0') { printf("\n"); return; }

    /* Headings */
    if (*p == '#' && !in_code_block) {
        int lvl = 0;
        while (*p == '#') { lvl++; p++; }
        if (*p == ' ' || *p == '\t') {
            while (*p && isspace((unsigned char)*p)) p++;
            ansi_fg_8bit(heading_color(lvl));
            printf(BOLD);
            if (lvl == 1)
                printf("[%d] ", lvl);
            else if (lvl == 2)
                printf("{%d} ", lvl);
            else
                printf("(%d) ", lvl);
            render_inline(p, expand_links);
            printf(RESET "\n");
            return;
        }
    }

    /* Unordered lists */
    if (!in_code_block && (*p == '-' || *p == '*') && isspace((unsigned char)p[1])) {
        ansi_fg_8bit(list_color());
        printf("  • ");
        ansi_fg_reset();
        p = skip_leading_spaces(p + 1);
        render_inline(p, expand_links);
        printf("\n");
        return;
    }

    /* Blockquote */
    if (!in_code_block && *p == '>') {
        printf(ESC "[90m│ " RESET);
        p++;
        if (*p == ' ') p++;
        render_inline(p, expand_links);
        printf("\n");
        return;
    }
    
    /* Normal paragraph */
    render_inline(p, expand_links);
    printf("\n");
}

void version(void) {
    printf("mdview %s\n\tCopyright (C) 2026 %s\n\tThis is free software; you are free to change and redistribute it.\n\tThere is NO WARRANTY, to the extent permitted by law.\n", VERSION, AUTHOR);
}

void usage(void) {

}

int parse_cmdline_flags(int argc, char **argv, Flags *flags) {
    // Check if minimum number of arguments are present.
    if (argc < 2) {
		fprintf(stderr, "%s <file.md>\n", argv[0]);
		return 1;
    }
    // Parse all the relevant command line flags
    for (int i=1; i<argc; i++) {
        char *flag = argv[i];
        if (strcmp("-v", flag) == 0 || strcmp("--version", flag) == 0) {
            flags->show_version = true;
            return 0;
        } else if (strcmp("-h", flag) == 0 || strcmp("--help", flag) == 0) {
            flags->show_usage = true;
            return 0;
        } else if (strcmp("-x", flag) == 0 || strcmp("--expand-links", flag) == 0) {
            flags->expand_links = true;
        } else if (strcmp("-l", flag) == 0 || strcmp("--less", flag) == 0) {
            flags->paged_output = true;
        } else {
            flags->mdfile_path = flag;
            break;
        }
    }
    // Check if the file provided has a valid extension
    char* ext = strchr(flags->mdfile_path, '.');
    if (ext == NULL) {
        fprintf(stderr, "File does not have any extension ... Is it a markdown file ?");
        return 2;
    }
    // Invalid file extension !!
    if (strcmp(ext+1, "md") != 0) {
        fprintf(stderr, "Invalid file extension %s", ext);
        return 3;
    } 
    return 0;
}

/* --- MAIN --------------------------------------------------------------- */
int main(int argc, char **argv) {
    Flags flags  = { 0 };

    int res = parse_cmdline_flags(argc, argv, &flags);
    if (res != 0) {
        return res;
    }

    if (flags.show_version) {
        version();
        return 0;
    } else if (flags.show_usage) {
        usage();
        return 0;
    }

    FILE *f = stdin;
    if (argc >= 2) {
        f = fopen(flags.mdfile_path, "r");
        if (!f) { perror("fopen"); return 2; }
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, f)) != -1) {
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        process_line(line, flags.expand_links);
    }

    if (flags.paged_output) { printf("Paged output not yet supported !!\n"); }

    free(line);
    if (f != stdin) fclose(f);
    return 0;
}

/*
Revision History:
    1.0.0: Initial Build.
    1.0.1: Added version flag and info
    1.0.2 (13-07-2026): Initial Release ! Added author info and copyright notice
    1.1.0 (17-07-2026): 
        - Fixed bug which was causing code blocks to render markdown inside them. Now code blocks are rendered raw.
        - Code blocks now show the language if specified after the opening backticks.
        - `--less` flag added to show a paged output for the file (similar to piping to the less command). This is useful for large files.
        - Links are now rendered in blue color and underlined. Use flag `--expand-links` to expand links to their full URL.
*/
