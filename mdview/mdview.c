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

#define VERSION "1.0.2"
#define AUTHOR "Satwik Srivastava"

/* ANSI helpers */
#define ESC "\x1b"
#define RESET ESC "[0m"
#define BOLD  ESC "[1m"
#define BOLD_OFF ESC "[22m"
#define ITALIC ESC "[3m"
#define ITALIC_OFF ESC "[23m"
#define REVERSE ESC "[7m"
#define REVERSE_OFF ESC "[27m"

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

/* --- INLINE RENDERER --------------------------------------------------- */
/*
   Supports:
   - *text* → italic
   - **text** → bold
   - `code` → reverse video
*/
static void render_inline(const char *s) {
    size_t i = 0;
    size_t n = strlen(s);
    int bold = 0, italic = 0, rev = 0;

    while (i < n) {
        char c = s[i];

        /* Inline code: `code` */
        if (c == '`') {
            if (!rev) { printf(REVERSE); rev = 1; }
            else { printf(REVERSE_OFF); rev = 0; }
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

        putchar(c);
        i++;
    }

    /* safety: close any open styles */
    if (rev) printf(REVERSE_OFF);
    if (bold) printf(BOLD_OFF);
    if (italic) printf(ITALIC_OFF);
}

/* Trim leading spaces */
static const char *skip_leading_spaces(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* --- LINE-LEVEL MARKDOWN ------------------------------------------------ */
static void process_line(const char *line) {
    const char *p = skip_leading_spaces(line);

    if (*p == '\0') { printf("\n"); return; }

    /* Headings */
    if (*p == '#') {
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
            render_inline(p);
            printf(RESET "\n");
            return;
        }
    }

    /* Unordered lists */
    if ((*p == '-' || *p == '*') && isspace((unsigned char)p[1])) {
        ansi_fg_8bit(list_color());
        printf("  • ");
        ansi_fg_reset();
        p = skip_leading_spaces(p + 1);
        render_inline(p);
        printf("\n");
        return;
    }

    /* Blockquote */
    if (*p == '>') {
        printf(ESC "[90m│ " RESET);
        p++;
        if (*p == ' ') p++;
        render_inline(p);
        printf("\n");
        return;
    }

    /* Fenced code blocks (just divider lines for now) */
    if (strncmp(p, "```", 3) == 0) {
        printf(ESC "[90m──────── code block ────────" RESET "\n");
        return;
    }

    /* Normal paragraph */
    render_inline(p);
    printf("\n");
}

void version(void) {
    printf("mdview %s\n\tCopyright (C) 2026 %s\n\tThis is free software; you are free to change and redistribute it.\n\tThere is NO WARRANTY, to the extent permitted by law.\n", VERSION, AUTHOR);
}

/* --- MAIN --------------------------------------------------------------- */
int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "%s <file.md>\n", argv[0]);
		return 1;
	}
    if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		version();
        return 0;
    }

    FILE *f = stdin;
    if (argc >= 2) {
        f = fopen(argv[1], "r");
        if (!f) { perror("fopen"); return 2; }
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, f)) != -1) {
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        process_line(line);
    }

    free(line);
    if (f != stdin) fclose(f);
    return 0;
}

/*
Revision History:
    1.0.0: Initial Build.
    1.0.1: Added version flag and info
    1.0.2 (13-07-2026): Initial Release ! Added author info and copyright notice
*/
