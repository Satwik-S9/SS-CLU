/*
 * csview — CSV pretty-printer
 * Author: Satwik Srivastava
 *
 * Usage:  csview [OPTIONS] [FILE]
 *         cat file.csv | csview [OPTIONS]
 *
 * Options:
 *   -c, --color         Colorize each column with ANSI colors
 *   -H, --head [N]      Show first N data rows (default: 5)
 *   -T, --tail [N]      Show last  N data rows (default: 5)
 *       --no-header     Treat first row as data, not as a header
 *   -h, --help          Show this help and exit
 *   -v, --version		 Show version information
 *
 * No external dependencies — only the C standard library.
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  ifndef _CRT_NONSTDC_NO_WARNINGS
#    define _CRT_NONSTDC_NO_WARNINGS
#  endif
#  include <windows.h>
#  define strtok_r strtok_s
#else
#  define _POSIX_C_SOURCE 200809L
#  include <unistd.h>
#  include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define VERSION "1.1.0"
#define AUTHOR "Satwik Srivastava"

#define DEFAULT_N  5

/* ── ANSI escape codes ──────────────────────────────────────────── */
#define A_RESET   "\033[0m"
#define A_BORDER  "\033[90m"    /* dark grey  */
#define A_HEADER  "\033[1;97m"  /* bold white */

static const char *const A_COL[] = {
    "\033[96m",   /* bright cyan    */
    "\033[93m",   /* bright yellow  */
    "\033[95m",   /* bright magenta */
    "\033[94m",   /* bright blue    */
    "\033[92m",   /* bright green   */
    "\033[91m",   /* bright red     */
};
#define NCOL ((int)(sizeof A_COL / sizeof *A_COL))

/* ── Box-drawing characters (UTF-8) ────────────────────────────── */
#define B_H   "─"   /* horizontal              */
#define B_V   "│"   /* vertical                */
#define B_TL  "╭"   /* top-left  (rounded)     */
#define B_TR  "╮"   /* top-right (rounded)     */
#define B_BL  "╰"   /* bot-left  (rounded)     */
#define B_BR  "╯"   /* bot-right (rounded)     */
#define B_TM  "┬"   /* top    T-intersection   */
#define B_BM  "┴"   /* bottom T-intersection   */
#define B_LM  "├"   /* left   T-intersection   */
#define B_RM  "┤"   /* right  T-intersection   */
#define B_X   "┼"   /* cross  intersection     */

void version(void) {
    printf("csview %s\n\tCopyright (C) 2026 %s\n\tThis is free software; you are free to change and redistribute it.\n\tThere is NO WARRANTY, to the extent permitted by law.\n", VERSION, AUTHOR);
}

/* ══════════════════════════════════════════════════════════════════
 * Dynamic string
 * ══════════════════════════════════════════════════════════════════ */
typedef struct { char *d; size_t n, c; } Dstr;

static void dstr_reset(Dstr *s)
{
    s->n = 0;
    if (s->d) s->d[0] = '\0';
}

static void dstr_free(Dstr *s)
{
    free(s->d);
    s->d = NULL; s->n = s->c = 0;
}

static int dstr_push(Dstr *s, char ch)
{
    if (s->n + 2 > s->c) {
        size_t nc = s->c ? s->c * 2 : 128;
        char  *p  = realloc(s->d, nc);
        if (!p) return 0;
        s->d = p; s->c = nc;
    }
    s->d[s->n++] = ch;
    s->d[s->n]   = '\0';
    return 1;
}

/* Transfer ownership of the internal buffer to the caller (must free). */
static char *dstr_take(Dstr *s)
{
    char *r  = s->d ? s->d : strdup("");
    s->d     = NULL;
    s->n = s->c = 0;
    return r;
}

/* ══════════════════════════════════════════════════════════════════
 * CSV types
 * ══════════════════════════════════════════════════════════════════ */
typedef struct { char **f; int n; } Row;
typedef struct { Row *r; int n, cap, ncol; } Table;

static int row_push(Row *row, char *s)
{
    char **t = realloc(row->f, (size_t)(row->n + 1) * sizeof *t);
    if (!t) return 0;
    row->f = t;
    row->f[row->n++] = s;
    return 1;
}

static void row_free(Row *row)
{
    for (int i = 0; i < row->n; i++) free(row->f[i]);
    free(row->f);
    row->f = NULL; row->n = 0;
}

static int tbl_push(Table *t, Row row)
{
    if (t->n == t->cap) {
        int  nc = t->cap ? t->cap * 2 : 256;
        Row *p  = realloc(t->r, (size_t)nc * sizeof *p);
        if (!p) return 0;
        t->r = p; t->cap = nc;
    }
    if (row.n > t->ncol) t->ncol = row.n;
    t->r[t->n++] = row;
    return 1;
}

static void tbl_free(Table *t)
{
    for (int i = 0; i < t->n; i++) row_free(&t->r[i]);
    free(t->r);
    t->r = NULL; t->n = t->cap = t->ncol = 0;
}

/* ══════════════════════════════════════════════════════════════════
 * CSV parser
 *
 * Returns:  1 = row parsed successfully
 *           0 = EOF (no more data)
 *          -1 = allocation error
 * ══════════════════════════════════════════════════════════════════ */
static int csv_read_row(FILE *fp, Row *out)
{
    *out = (Row){NULL, 0};
    Dstr f = {NULL, 0, 0};
    int  c;

    /* Skip blank / empty lines between records */
    do { c = fgetc(fp); } while (c == '\r' || c == '\n');
    if (c == EOF) return 0;

    do {
        dstr_reset(&f);

        if (c == '"') {
            /* ── quoted field ── */
            c = fgetc(fp);
            while (c != EOF) {
                if (c == '"') {
                    int nx = fgetc(fp);
                    if (nx == '"') {
                        /* escaped double-quote ("") */
                        if (!dstr_push(&f, '"')) goto oom;
                        c = fgetc(fp);
                    } else {
                        /* end of quoted section; nx is the separator */
                        c = nx;
                        break;
                    }
                } else {
                    if (!dstr_push(&f, (char)c)) goto oom;
                    c = fgetc(fp);
                }
            }
        } else {
            /* ── unquoted field ── */
            while (c != EOF && c != ',' && c != '\n' && c != '\r') {
                if (!dstr_push(&f, (char)c)) goto oom;
                c = fgetc(fp);
            }
        }

        char *s = dstr_take(&f);
        if (!s || !row_push(out, s)) { free(s); goto oom; }

        if (c == ',') c = fgetc(fp);   /* advance to start of next field */

    } while (c != EOF && c != '\n' && c != '\r');

    /* Consume trailing CR in CRLF */
    if (c == '\r') {
        int nx = fgetc(fp);
        if (nx != '\n' && nx != EOF) ungetc(nx, fp);
    }

    dstr_free(&f);
    return 1;

oom:
    dstr_free(&f);
    row_free(out);
    return -1;
}

/* ══════════════════════════════════════════════════════════════════
 * Table rendering
 * ══════════════════════════════════════════════════════════════════ */

#define ELLIPSIS   "..."
#define ELLIPSIS_W 3

/* Return the current terminal width in columns, falling back to the
 * COLUMNS environment variable, and finally to 80. */
static int get_term_width(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
#endif
    const char *cols = getenv("COLUMNS");
    if (cols && *cols) {
        int v = atoi(cols);
        if (v > 0) return v;
    }
    return 80;
}

/* A header, wrapped onto at most two display lines. */
typedef struct { char *l1, *l2; } HdrLines;

/* Wrap a header string onto (up to) two lines by splitting on spaces.
 * The column only needs to be as wide as the longest single word
 * ("sub-word"): the first line is greedily packed with words up to
 * that width, and any remaining words spill onto the second line.
 * *width_out receives the resulting column width requirement. */
static void wrap_header(const char *hdr, HdrLines *out, int *width_out)
{
    if (!hdr) hdr = "";
    out->l1 = NULL;
    out->l2 = NULL;

    char *tmp = strdup(hdr);
    if (!tmp) { out->l1 = strdup(""); *width_out = 0; return; }

    enum { MAXWORDS = 128 };
    char *words[MAXWORDS];
    int   nw = 0;
    char *save = NULL;
    for (char *tok = strtok_r(tmp, " ", &save);
         tok && nw < MAXWORDS;
         tok = strtok_r(NULL, " ", &save))
        words[nw++] = tok;

    if (nw <= 1) {
        out->l1 = strdup(hdr);
        *width_out = (int)strlen(hdr);
        free(tmp);
        return;
    }

    int maxword = 0;
    for (int i = 0; i < nw; i++) {
        int l = (int)strlen(words[i]);
        if (l > maxword) maxword = l;
    }

    Dstr l1 = {0}, l2 = {0};
    int i = 0;
    for (; i < nw; i++) {
        int wl  = (int)strlen(words[i]);
        int cur = (int)l1.n;
        int need = cur == 0 ? wl : cur + 1 + wl;
        if (cur > 0 && need > maxword) break;
        if (cur > 0) dstr_push(&l1, ' ');
        for (const char *p = words[i]; *p; p++) dstr_push(&l1, *p);
    }
    for (; i < nw; i++) {
        if (l2.n > 0) dstr_push(&l2, ' ');
        for (const char *p = words[i]; *p; p++) dstr_push(&l2, *p);
    }

    out->l1 = dstr_take(&l1);
    out->l2 = (l2.n > 0) ? dstr_take(&l2) : NULL;
    if (!l2.n) dstr_free(&l2);

    int w1 = (int)strlen(out->l1);
    int w2 = out->l2 ? (int)strlen(out->l2) : 0;
    *width_out = w1 > w2 ? w1 : w2;
    if (*width_out < maxword) *width_out = maxword;

    free(tmp);
}

static void free_hdr_lines(HdrLines *hdr, int ncol)
{
    if (!hdr) return;
    for (int j = 0; j < ncol; j++) { free(hdr[j].l1); free(hdr[j].l2); }
    free(hdr);
}

/* Compute per-column widths over the header row (if show_hdr) and
 * data rows [from..to]. The header's contribution to the width is
 * based on its wrapped (max sub-word) form, not its raw length, so
 * that multi-word headers don't force columns to be unnecessarily
 * wide. *hdr_out receives the wrapped header lines (only meaningful
 * when show_hdr is true) and *hdr_nlines_out the number of header
 * display lines needed (1 or 2). */
static int *compute_layout(const Table *t, int from, int to, int show_hdr,
                           HdrLines **hdr_out, int *hdr_nlines_out)
{
    int ncol = t->ncol;
    int *w = calloc((size_t)ncol, sizeof *w);
    HdrLines *hdr = calloc((size_t)ncol, sizeof *hdr);
    if (!w || !hdr) { free(w); free(hdr); *hdr_out = NULL; return NULL; }

    int nlines = 1;
    if (show_hdr && t->n > 0) {
        const Row *hr = &t->r[0];
        for (int j = 0; j < ncol; j++) {
            const char *cell = (j < hr->n) ? hr->f[j] : "";
            int hw;
            wrap_header(cell, &hdr[j], &hw);
            if (hdr[j].l2) nlines = 2;
            if (hw > w[j]) w[j] = hw;
        }
    }

    for (int i = from; i <= to && i < t->n; i++) {
        const Row *row = &t->r[i];
        for (int j = 0; j < row->n; j++) {
            int l = (int)strlen(row->f[j]);
            if (l > w[j]) w[j] = l;
        }
    }
    for (int j = 0; j < ncol; j++)
        if (w[j] == 0) w[j] = 1;

    *hdr_out = hdr;
    *hdr_nlines_out = nlines;
    return w;
}

/* Total rendered width (in terminal columns) for ndisplay slots of
 * the given widths, including borders and padding. */
static int total_width_for(const int *dispw, int ndisplay)
{
    int total = ndisplay + 1;
    for (int s = 0; s < ndisplay; s++) total += dispw[s] + 2;
    return total;
}

/* Print a horizontal rule using UTF-8 box-drawing strings. */
static void hline(const int *dispw, int ndisplay,
                  const char *lc, const char *mc, const char *hc,
                  const char *rc, int clr)
{
    if (clr) fputs(A_BORDER, stdout);
    fputs(lc, stdout);
    for (int s = 0; s < ndisplay; s++) {
        for (int k = 0; k < dispw[s] + 2; k++) fputs(hc, stdout);
        fputs(s == ndisplay - 1 ? rc : mc, stdout);
    }
    if (clr) fputs(A_RESET, stdout);
    putchar('\n');
}

/* Print one line of the (possibly two-line) header. colmap[s] is the
 * real column index for slot s, or -1 for the omitted-columns
 * ellipsis slot. which_line selects the first or second header line. */
static void print_header_line(const HdrLines *hdr, const int *dispw,
                              const int *colmap, int ndisplay,
                              int which_line, int clr)
{
    for (int s = 0; s < ndisplay; s++) {
        int j = colmap[s];
        const char *cell;
        if (j < 0)
            cell = (which_line == 0) ? ELLIPSIS : "";
        else
            cell = which_line == 0 ? (hdr[j].l1 ? hdr[j].l1 : "")
                                    : (hdr[j].l2 ? hdr[j].l2 : "");
        int len = (int)strlen(cell);
        int pad = dispw[s] - len;
        if (pad < 0) pad = 0;

        if (clr) fputs(A_BORDER, stdout);
        fputs(B_V, stdout);
        putchar(' ');
        if (clr) fputs(A_HEADER, stdout);
        fputs(cell, stdout);
        for (int k = 0; k < pad; k++) putchar(' ');
        if (clr) fputs(A_RESET, stdout);
        putchar(' ');
    }
    if (clr) fputs(A_BORDER, stdout);
    fputs(B_V, stdout);
    if (clr) fputs(A_RESET, stdout);
    putchar('\n');
}

/* Print one data row. colmap[s] is the real column index for slot s,
 * or -1 for the omitted-columns ellipsis slot. */
static void print_data_row(const Row *row, const int *dispw,
                           const int *colmap, int ndisplay, int clr)
{
    for (int s = 0; s < ndisplay; s++) {
        int j = colmap[s];
        const char *cell = (j < 0) ? ELLIPSIS
                                   : ((j < row->n) ? row->f[j] : "");
        int len = (int)strlen(cell);
        int pad = dispw[s] - len;
        if (pad < 0) pad = 0;

        if (clr) fputs(A_BORDER, stdout);
        fputs(B_V, stdout);
        putchar(' ');
        if (clr) fputs(j >= 0 ? A_COL[j % NCOL] : A_BORDER, stdout);
        fputs(cell, stdout);
        for (int k = 0; k < pad; k++) putchar(' ');
        if (clr) fputs(A_RESET, stdout);
        putchar(' ');
    }
    if (clr) fputs(A_BORDER, stdout);
    fputs(B_V, stdout);
    if (clr) fputs(A_RESET, stdout);
    putchar('\n');
}

/* Decide which columns to display when the full table doesn't fit
 * the terminal: greedily take columns from the left and right ends
 * (whichever still fits) and collapse whatever remains in the middle
 * into a single "..." column. Builds colmap/dispw (caller frees) and
 * returns the number of display slots. */
static int fit_columns(const int *w, int ncol, int term_w,
                       int **colmap_out, int **dispw_out)
{
    int running = 1 + (ELLIPSIS_W + 2 + 1); /* opening border + ellipsis slot */
    int li = 0, ri = ncol - 1;
    char *take = calloc((size_t)ncol, 1); /* 0=skip, 1=left, 2=right */

    while (li <= ri) {
        int added = 0;
        if (li <= ri) {
            int cost = w[li] + 2 + 1;
            if (running + cost <= term_w) {
                take[li] = 1; running += cost; li++; added = 1;
            }
        }
        if (li <= ri) {
            int cost = w[ri] + 2 + 1;
            if (running + cost <= term_w) {
                take[ri] = 2; running += cost; ri--; added = 1;
            }
        }
        if (!added) break;
    }

    int nleft = 0, nright = 0;
    for (int j = 0; j < ncol; j++) {
        if (take[j] == 1) nleft++;
        else if (take[j] == 2) nright++;
    }
    if (nleft == 0 && nright == 0 && ncol > 0) { take[0] = 1; nleft = 1; }

    int ndisplay = nleft + 1 + nright;
    int *colmap = malloc((size_t)ndisplay * sizeof *colmap);
    int *dispw  = malloc((size_t)ndisplay * sizeof *dispw);

    int idx = 0;
    for (int j = 0; j < ncol; j++)
        if (take[j] == 1) { colmap[idx] = j; dispw[idx] = w[j]; idx++; }
    colmap[idx] = -1; dispw[idx] = ELLIPSIS_W; idx++;
    for (int j = 0; j < ncol; j++)
        if (take[j] == 2) { colmap[idx] = j; dispw[idx] = w[j]; idx++; }

    free(take);
    *colmap_out = colmap;
    *dispw_out  = dispw;
    return ndisplay;
}

/* Render the table, showing the header (if has_hdr) and data rows
 * in the range [from..to]. */
static void render(const Table *t, int from, int to,
                   int has_hdr, int clr)
{
    int show_hdr  = has_hdr && t->n > 0;
    int show_data = (from <= to) && (from < t->n);

    if (!show_hdr && !show_data) {
        puts("(empty)");
        return;
    }

    /* Widths are calculated only over the visible rows. Headers are
     * wrapped onto up to two lines so that a column only needs to be
     * as wide as its longest single word — this keeps columns as
     * narrow as possible and makes the table easier to fit. */
    HdrLines *hdr = NULL;
    int hdr_nlines = 1;
    int ncol = t->ncol;
    int *w = compute_layout(t,
                            show_data ? from : 0,
                            show_data ? to   : -1,
                            show_hdr, &hdr, &hdr_nlines);
    if (!w) { fputs("error: out of memory\n", stderr); return; }

    int *colmap, *dispw, ndisplay;
    int term_w = get_term_width();

    if (total_width_for(w, ncol) <= term_w) {
        /* Everything fits — show every column as-is. */
        colmap = malloc((size_t)ncol * sizeof *colmap);
        dispw  = malloc((size_t)ncol * sizeof *dispw);
        for (int j = 0; j < ncol; j++) { colmap[j] = j; dispw[j] = w[j]; }
        ndisplay = ncol;
    } else {
        /* Doesn't fit even with wrapped headers — drop columns from
         * the middle and mark the gap with a "..." column. */
        ndisplay = fit_columns(w, ncol, term_w, &colmap, &dispw);
        fprintf(stderr,
                "[csview] table too wide for terminal (%d cols); "
                "showing %d of %d columns\n",
                term_w, ndisplay - 1, ncol);
    }

    /* top border */
    hline(dispw, ndisplay, B_TL, B_TM, B_H, B_TR, clr);

    /* header row(s) */
    if (show_hdr) {
        print_header_line(hdr, dispw, colmap, ndisplay, 0, clr);
        if (hdr_nlines > 1)
            print_header_line(hdr, dispw, colmap, ndisplay, 1, clr);
        if (show_data)
            hline(dispw, ndisplay, B_LM, B_X, B_H, B_RM, clr);
        else
            hline(dispw, ndisplay, B_BL, B_BM, B_H, B_BR, clr);
    }

    /* data rows — continuous (no separators between rows) */
    if (show_data) {
        for (int i = from; i <= to && i < t->n; i++)
            print_data_row(&t->r[i], dispw, colmap, ndisplay, clr);
        /* bottom border */
        hline(dispw, ndisplay, B_BL, B_BM, B_H, B_BR, clr);
    }

    free(w);
    free(colmap);
    free(dispw);
    free_hdr_lines(hdr, ncol);
}

/* ══════════════════════════════════════════════════════════════════
 * CLI helpers
 * ══════════════════════════════════════════════════════════════════ */
static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS] [FILE]\n\n"
        "Pretty-print a CSV file as a table.\n"
        "Reads from stdin when FILE is omitted.\n\n"
        "Options:\n"
        "  -c, --color         Colorize columns with ANSI escape codes\n"
        "  -H, --head [N]      Show first N data rows (default: %d)\n"
        "  -T, --tail [N]      Show last  N data rows (default: %d)\n"
        "      --no-header     Treat first row as data, not as a header\n"
        "  -h, --help          Show this help and exit\n\n"
        "Examples:\n"
        "  %s data.csv\n"
        "  %s --color --head 10 data.csv\n"
        "  %s --tail data.csv\n"
        "  cat data.csv | %s --color\n",
        prog, DEFAULT_N, DEFAULT_N, prog, prog, prog, prog);
}

/* Return 1 if 's' is a string of digits representing a positive
 * integer in [1, 1000000], placing the value in *out. */
static int is_posint(const char *s, int *out)
{
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++)
        if (*p < '0' || *p > '9') return 0;
    long v = strtol(s, NULL, 10);
    if (v <= 0 || v > 1000000) return 0;
    *out = (int)v;
    return 1;
}

/* ══════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════ */
typedef enum { M_ALL, M_HEAD, M_TAIL } Mode;

int main(int argc, char *argv[])
{
    int         use_color = 0;
    int         has_hdr   = 1;
    Mode        mode      = M_ALL;
    int         n         = DEFAULT_N;
    const char *file      = NULL;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

    /* ── parse arguments ── */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(argv[0]);
            return 0;

        } else if (!strcmp(a, "-c") || !strcmp(a, "--color")) {
            use_color = 1;

        } else if (!strcmp(a, "--no-header")) {
            has_hdr = 0;

        } else if (!strcmp(a, "-H") || !strcmp(a, "--head")) {
            mode = M_HEAD;
            int tmp;
            if (i + 1 < argc && is_posint(argv[i + 1], &tmp))
                { n = tmp; i++; }
            else
                n = DEFAULT_N;

        } else if (!strcmp(a, "-T") || !strcmp(a, "--tail")) {
            mode = M_TAIL;
            int tmp;
            if (i + 1 < argc && is_posint(argv[i + 1], &tmp))
                { n = tmp; i++; }
            else
                n = DEFAULT_N;

        } else if (!strcmp(a, "--version") || !strcmp(a, "-v")) {
            version();
            return 0;
        } else if (a[0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", a);
            usage(argv[0]);
            return 1;

        } else {
            if (file) {
                fputs("error: only one input file may be specified\n", stderr);
                return 1;
            }
            file = a;
        }
    }

    /* ── open input ── */
    FILE *fp = stdin;
    if (file) {
        fp = fopen(file, "r");
        if (!fp) {
            fprintf(stderr, "error: cannot open '%s': %s\n",
                    file, strerror(errno));
            return 1;
        }
    }

    /* ── read all rows ── */
    Table t = {NULL, 0, 0, 0};
    Row   row;
    int   rc;

    while ((rc = csv_read_row(fp, &row)) == 1) {
        if (!tbl_push(&t, row)) {
            row_free(&row);
            fputs("error: out of memory\n", stderr);
            if (file) fclose(fp);
            tbl_free(&t);
            return 1;
        }
    }
    if (file) fclose(fp);

    if (rc == -1) {
        fputs("error: out of memory while reading input\n", stderr);
        tbl_free(&t);
        return 1;
    }

    /* ── compute display range ── */
    int data_start = has_hdr ? 1 : 0;
    int data_end   = t.n - 1;
    int ndata      = (t.n > data_start) ? (t.n - data_start) : 0;

    int from, to;
    switch (mode) {
    case M_HEAD:
        from = data_start;
        to   = data_start + n - 1;
        if (to > data_end) to = data_end;
        break;
    case M_TAIL:
        to   = data_end;
        from = to - n + 1;
        if (from < data_start) from = data_start;
        break;
    default: /* M_ALL */
        from = data_start;
        to   = data_end;
        break;
    }

    /* Print a summary to stderr when head/tail is active */
    if (mode != M_ALL) {
        int shown = (from <= to) ? (to - from + 1) : 0;
        fprintf(stderr, "[%s %d]  showing %d of %d data row(s)\n",
                mode == M_HEAD ? "head" : "tail", n, shown, ndata);
    }

    render(&t, from, to, has_hdr, use_color);
    tbl_free(&t);
    return 0;
}

/*
Revision History:
    1.0.0: Initial Build.
    1.0.1: Added version flag and info
    1.0.2: Added author info and copyright notice

    1.1.0 (13-07-2026): First Release ! Added support to check terminal width and adjust the number of columns displayed accordingly.

    1.2.0 (17-07-2026): Updated to support windows builds.
*/
