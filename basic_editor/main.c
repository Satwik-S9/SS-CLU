/* Includes */
#include <limits.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

#include "config.h"

/* Defines */

#define _BSD_SOURCE
#define _GNU_SOURCE

#ifndef NAME_MAX
	#define NAME_MAX 255
#endif

#ifndef PATH_MAX
	#define PATH_MAX 4096
#endif

#define CTRL_KEY(k) ((k) & 0x1f)
#define BASIC_EDITOR_VERSION "1.0.0"

#define B_H   "─"   /* horizontal              */
#define B_V   "│"   /* vertical                */
#define B_TL  "╭"   /* top-left  (rounded)     */
#define B_TR  "╮"   /* top-right (rounded)     */
#define B_BL  "╰"   /* bot-left  (rounded)     */
#define B_BR  "╯"   /* bot-right (rounded)     */
#define B_LT "├"
#define B_RT "┤"

#define MAX_SPANS 6

// Highlights for Homepage :: To be moved to a separate config file
#ifdef LIGHT_THEME_MODE
	#define S_KEY  "38;2;150;150;150"
	#define S_TEXT "38;2;166;166;166"
#endif

#ifdef DARK_THEME_MODE
	#define S_KEY    "1;38;5;252"
	#define S_TEXT   "38;5;250"
#endif

#define S_DIM    "38;5;240"
#define S_ACCENT "38;2;133;153;0"
#define S_RESET  "\x1b[0m"

/** Palette Highlights **/
#ifdef LIGHT_THEME_MODE
	#define PAL_DIM    "\x1b[38;2;190;190;190m" /* faint border/hint grey, barely off-white     */
	#define PAL_TEXT   "\x1b[38;2;110;110;110m" /* regular command-name grey, readable on white */
	#define PAL_BOLD   "\x1b[1;38;2;20;20;20m"  /* near-black bold, for what you've typed       */
	#define PAL_SEL    "\x1b[48;2;225;225;225m" /* soft grey row highlight (matches STATUS_BAR_BACKGROUND) */
#endif

#ifdef DARK_THEME_MODE
	#define PAL_DIM    "\x1b[38;5;240m"
	#define PAL_TEXT   "\x1b[38;5;250m"
	#define PAL_BOLD   "\x1b[1;38;5;253m"
	#define PAL_SEL    "\x1b[48;5;236m"
#endif

#define PAL_RESET  "\x1b[0m"
#define PAL_ACCENT "\x1b[38;2;133;153;0m"

// Mod Keys !!
#define MOD_SHIFT 0x01
#define MOD_ALT   0x02
#define MOD_CTRL  0x04

// Default Tab Size
#define MAX_INPUT_SIZE 	  256
#define DEFAULT_TAB_SIZE  4

// Gutter: "%5d" + "\u2502" + " " == 7 cells
#define GUTTER_WIDTH 7

// Load Statusbar theme
static struct StatusBar_Theme sb_theme = { STATUS_BAR_BACKGROUND, STATUS_BAR_FOREGROUND};

/* Data Models */

/** Generic Data Models **/
/* String Builder */
typedef struct {
	char *data;
	size_t len;
	size_t cap;
} StringBuilder;


/** BE Specific Data Models **/
typedef enum {
	BE_ERR_ASSERT,
	BE_ERR_RENDER,
	BE_ERR_TERM,
	BE_ERR_OP
} BE_ErrorKind;


typedef enum {
	// Keys present in ASCII
	BACKSPACE = 0x7f,
    CTRL_BACKSPACE = 0x08,

    // Other Keys
    ARROW_LEFT  = 1000,
	ARROW_RIGHT,
	ARROW_DOWN,
	ARROW_UP,
	PAGE_UP,
	PAGE_DOWN,
	HOME,
	END,
	CTRL_HOME,
	CTRL_END,
	DELETE,
	WORD_DELETE,
	WORD_RIGHT,
	WORD_LEFT,

	CTRL_SHIFT_S,
    ALT_A,
    ALT_E
} BE_Key;


typedef enum {
	AL_CENTER,
	AL_BLOCK
} BE_Align;

typedef enum {
	SPLASH,
	EDIT
} BE_Mode;

typedef struct {
	const char *style;
	const char *text;
} BE_Span;


typedef struct {
	BE_Span  spans[MAX_SPANS];
	int 	 nspans;
	BE_Align align;
} BE_HomepageLine;


/* Homepage Lines */
static const BE_HomepageLine homepage[] = {
	{ {{S_DIM, "▄▄▄  ▄  ▄  ▄   ▄"}},                                    1, AL_CENTER },
	{ {{NULL, "\r\n"}},                                                     1, AL_CENTER },
    { {{S_KEY,"be"},{S_TEXT," — a basic editor   "},
	   {S_DIM,"v" BASIC_EDITOR_VERSION}},                               3, AL_CENTER },
	{ {{S_DIM, "quick edits, no hassle."}},                             1, AL_CENTER },
	{ {{NULL, "\r\n"}},                                                     1, AL_CENTER },
	{ {{S_DIM, "── functions & keymaps ──"}},                           1, AL_CENTER },
	{ {{NULL, ""}},                                                     1, AL_CENTER },
	{ {{S_KEY,"Ctrl+S  "},{S_TEXT,"save file\r\n"}},                        2, AL_BLOCK  },
	{ {{S_KEY,"Ctrl+Q  "},{S_TEXT,"quit — asks before losing changes\r\n"}},2, AL_BLOCK  },
	{ {{S_KEY,"Ctrl+P  "},{S_TEXT,"command palette · every function\r\n"}},  2, AL_BLOCK  },
	{ {{S_KEY,"Ctrl+F  "},{S_TEXT,"find in file\r\n"}},                     2, AL_BLOCK  },
	{ {{S_KEY,"Ctrl+G  "},{S_TEXT,"go to line\r\n"}},                       2, AL_BLOCK  },
	{ {{S_KEY,"Ctrl+O  "},{S_TEXT,"open a file · one buffer, one file\r\n"}},2, AL_BLOCK },
	{ {{S_KEY,"↑↓←→    "},{S_TEXT,"move · Home / End · PgUp / PgDn\r\n"}},   2, AL_BLOCK  },
	{ {{NULL, "\r\n"}},                                                     1, AL_CENTER },
	{ {{S_DIM,"press "},{S_KEY,"Enter"},
	   {S_DIM," to open a file — or "},{S_KEY,"Ctrl+P"},
	   {S_DIM," for commands"}},                                        5, AL_CENTER },
};

/* Save Dialog Structures */
typedef enum {
	SD_FOCUS_INPUT,
	SD_FOCUS_SAVE,
	SD_FOCUS_QUIT,
	SD_FOCUS_CANCEL,
	SD_FOCUS_COUNT
} BE_SaveDialogFocus;

typedef struct {
    bool   				active;
	bool   				quit_on_save;
	BE_SaveDialogFocus 	focus;
    char   				input_path[MAX_INPUT_SIZE + 1];
	size_t 				inputlen;
} BE_SaveDialog;

/* Open Dialog Box Structures */
typedef enum {
	OD_FOCUS_INPUT,
	OD_FOCUS_OPEN,
	OD_FOCUS_CANCEL,
	OD_FOCUS_COUNT,
} BE_OpenDialogFocus;

typedef struct {
	bool 				active;
	bool 				create_file;
	BE_OpenDialogFocus 	focus;
	char 				input_path[MAX_INPUT_SIZE + 1];
	size_t				inputlen;
} BE_OpenDialog;

typedef enum {
	GD_FOCUS_INPUT,
	GD_FOCUS_GOTO,
	GD_FOCUS_CANCEL,
	GD_FOCUS_COUNT,
} BE_GotoDialogFocus;

typedef struct {
	BE_GotoDialogFocus focus;
	bool active;
	int  inputlen;
	char input[32];
} BE_GotoDialog;

typedef struct {
	// int cx, cy;
	int size;
	int rsize;
	char *data;
	char *render;
} BE_Row;

typedef enum {
	CMD_SAVE = 0,
	CMD_SAVEAS,
	CMD_OPEN,
	CMD_QUIT,
	CMD_GOTO,
	CMD_GTST,
	CMD_GTEN,
	CMD_FIND,
} PaletteCmdType;

typedef struct {
	PaletteCmdType	cmd_type; 
	const char 		*name;
	const char 		*hint;
} BE_Command;

static const BE_Command commands[] = {
	{ CMD_SAVE, 	"save file",			"^S" },
	{ CMD_SAVEAS, 	"write file as",		"^⇧ S" },
	{ CMD_OPEN, 	"open file",			"^O" },
	{ CMD_QUIT, 	"quit", 				"^Q" },
	{ CMD_GOTO, 	"go to line", 			"^G" },
	{ CMD_GTST, 	"go to start of file",	"^Home" },
	{ CMD_GTEN, 	"go to end of file",    "^End" },
	{ CMD_FIND, 	"find in file", 		"^F" },
};

#define NCOMMANDS ((int)(sizeof(commands) / sizeof(commands[0])))

typedef struct {
	bool open;
	char query[MAX_INPUT_SIZE + 1];
	int  qlen;
	int  max_rows;
	int  sel;
	int  nfiltered;
	int  filtered[NCOMMANDS];
} BE_CmdPalette;

typedef struct {
	int    			def_x, def_y;
	int    			cur_x, cur_y;
	int     		row_x;
	int    			rowoff;
	int 			coloff;
	int    			screenrows;
	int    			screencols;
	int    			numrows;
	int	    		dirty;
	BE_Mode			mode;
	BE_GotoDialog   goto_dialog;
	BE_OpenDialog	open_dialog;
    BE_SaveDialog 	save_dialog;
	BE_CmdPalette   pal;
	BE_Row 			*row;
	char			*filename;
	char			statusmsg[80];
	time_t			statusmsg_time;
	struct termios 	orig_termios;
} BE_State;


static BE_State state;

// Forward Declarations for some functions: TODO: Move this to be.h
void be_quitEditor(void);
void be_quitNow(const char *message);
void be_die(const char *s);
void be_openBlankFile();
void be_editorInsertChar(int c);
bool be_saveFile(size_t *out_len);
void be_drawSaveDialog(StringBuilder *sb, int *cur_x, int *cur_y);
void be_deleteChar();
void be_insertNewLine();
bool validFilename(const char *filename);
bool checkFileExists(const char *filepath);
int isDir(const char *filename);
void be_setStatusMsg(const char *msg);
void be_openBlankFile();
void be_openFile(const char *filename);
void be_clearRows(void);
void be_updateRow(BE_Row *row);

/** String Builder Methods **/
void sb_init(StringBuilder *sb) {
        sb->cap = 16;
        sb->len = 0;
        sb->data = malloc(sb->cap);
        sb->data[0] = '\0';
}

void sb_append(StringBuilder *sb, const char *str) {
        size_t add_len = strlen(str);
        size_t needed = sb->len + add_len + 1;

        if (needed > sb->cap) {
                size_t new_cap = sb->cap ? sb->cap : 16; // escape the cap==0 trap
                while (new_cap < needed)
                        new_cap *= 2;
                char *new_data = realloc(sb->data, new_cap);
                if (!new_data) { free(sb->data); be_die("realloc"); }
                sb->data = new_data;
                sb->cap = new_cap;
        }

        memcpy(sb->data + sb->len, str, add_len + 1);
        sb->len += add_len;
}

void sb_free(StringBuilder *sb) {
        free(sb->data);
        sb->data = NULL;
        sb->len = sb->cap = 0;
}

/* Appends `n` raw bytes (need not be NUL-terminated, e.g. a slice of a
 * render buffer) instead of a C string. */
void sb_appendn(StringBuilder *sb, const char *str, size_t n) {
        size_t needed = sb->len + n + 1;

        if (needed > sb->cap) {
                size_t new_cap = sb->cap ? sb->cap : 16;
                while (new_cap < needed)
                        new_cap *= 2;
                char *new_data = realloc(sb->data, new_cap);
                if (!new_data) { free(sb->data); be_die("realloc"); }
                sb->data = new_data;
                sb->cap = new_cap;
        }

        memcpy(sb->data + sb->len, str, n);
        sb->len += n;
        sb->data[sb->len] = '\0';
}

void sb_repeat(StringBuilder *sb, const char *s, int n) {
	for (int i=0; i<n; i++) sb_append(sb, s);
}

/** Save Dialog Methods **/
void be_saveDialog_open(BE_SaveDialog *sd) {
	if (state.mode == SPLASH) {
		sd->active = false;
		return;
	}

    sd->active = true;
	sd->focus = SD_FOCUS_INPUT;
	sd->quit_on_save = false;

	// Pre-fill the input bar with the current filename, if any, so
	// re-saving an already-named file doesn't require retyping it.
	if (state.filename != NULL) {
		strncpy(sd->input_path, state.filename, MAX_INPUT_SIZE);
		sd->input_path[MAX_INPUT_SIZE] = '\0';
		sd->inputlen = strlen(sd->input_path);
	} else {
		sd->input_path[0] = '\0';
		sd->inputlen = 0;
	}
}


void be_saveDialog_close(BE_SaveDialog *sd) {
    sd->active = false;
	sd->focus = SD_FOCUS_INPUT;
	sd->inputlen = 0;
	sd->input_path[0] = '\0';
	sd->quit_on_save = false;
}

/* Open Dialog Methods */
void be_openDialog_open(BE_OpenDialog *od) {
	// If unsaved changes are present first prompt user to save
	if (state.dirty != 0) {
		be_saveDialog_open(&state.save_dialog);
		be_setStatusMsg("Please save the file first");
		return;
	}
	
	od->active = true;
	od->focus = OD_FOCUS_INPUT;
	od->inputlen = 0;
	od->input_path[0] = '\0';
}

void be_openDialog_close(BE_OpenDialog *od) {
	od->active = false;
	od->focus = OD_FOCUS_INPUT;
	od->inputlen = 0;
	od->input_path[0] = '\0';
}

void be_openDialog_commit() {
	BE_OpenDialog *od = &state.open_dialog;

	// Snapshot the path before closing: be_openDialog_close() wipes
	// od->input_path, so the read has to happen first.
	char filepath[MAX_INPUT_SIZE + 1];
	strncpy(filepath, od->input_path, MAX_INPUT_SIZE);
	filepath[MAX_INPUT_SIZE] = '\0';

	be_openDialog_close(od);

	if (!validFilename(filepath)) { be_setStatusMsg("{X} Invalid filename provided"); return; }

	int is_dir_res = isDir(filepath);
	if (is_dir_res == -1) { be_setStatusMsg("{X} Invalid filename provided"); return; }
	if (is_dir_res == -2) { be_setStatusMsg("{X} Error occured while opening"); return; }
	if (is_dir_res == 1)  { be_setStatusMsg("{X} Provided path is a directory"); return; }

	if (checkFileExists(filepath)) {
		be_openFile(filepath);
	} else {
		be_openBlankFile();
		state.filename = strdup(filepath);
	}
}

/* Goto Dialog Methods */
void be_gotoDialog_open(BE_GotoDialog *gd) {
	gd->active = true;
	gd->inputlen = 0;
	gd->input[0] = '\0';
	gd->focus = GD_FOCUS_INPUT;
}

void be_gotoDialog_close(BE_GotoDialog *gd) {
	gd->active = false;
	gd->inputlen = 0;
	gd->input[0] = '\0';
	gd->focus = GD_FOCUS_INPUT;
}

void be_gotoDialog_commit(BE_GotoDialog *gd) {
	if (gd->input[0] =='\0') {
		be_setStatusMsg("{X} No line no. provided");
		return;
	}

	char *endptr;
	errno = 0;

	long value = strtol(gd->input, &endptr, 10); // base 10

	if (endptr == gd->input) {
		be_setStatusMsg("{X} No line no. provided");
	} else if (errno == ERANGE) {
    	be_setStatusMsg("{X} Out of range ... Overflow|Underflow");
	} else if (*endptr != '\0') {
		be_setStatusMsg("{X} Invalid number !!");
	}

	int result = (int)value;
	be_gotoDialog_close(gd);

	if (result < 0) be_setStatusMsg("{X} Really ??");
	if (result >= state.numrows) {
		be_setStatusMsg("{X} Error line number is more than number of rows in the file");
	}

	state.cur_y = result-1;
	state.cur_x = 0;
}

/* Cmd Palette Methods */
static bool fuzzy_match(const char *hay, const char *needle) {
	if (!needle || *needle == '\0') return true;
	while (*hay) {
		if (tolower((unsigned char)*hay) == tolower((unsigned char)*needle)) {
			needle++;
			if (*needle == '\0') return true; // matched the whole needle as a subsequence
		}
		hay++;
	}
	return false;
}

static void be_paletteRefilter() {
	BE_CmdPalette *p = &state.pal;
	p->nfiltered = 0;
	for (int i = 0; i < NCOMMANDS; i++) {
		if (fuzzy_match(commands[i].name, p->query))
			p->filtered[p->nfiltered++] = i;
	}
	if (p->sel >= p->nfiltered) p->sel = p->nfiltered - 1;
	if (p->sel < 0) p->sel = 0;
}

static void be_paletteOpen(void) {
	BE_CmdPalette *p = &state.pal;
	p->open = true;
	p->qlen = 0;
	p->query[0] = '\0';
	p->sel = 0;
	be_paletteRefilter();
}

static void be_paletteClose() {
	state.pal.open = false;
}

static void be_paletteExecuteCmd(void) {
	BE_CmdPalette *p = &state.pal;

	if (p->nfiltered == 0 || p->sel < 0 || p->sel >= p->nfiltered) return;
	BE_Command cmd = commands[p->filtered[p->sel]];
	size_t outlen;
	bool ok;
	char msg[80];
	switch (cmd.cmd_type) {
		case CMD_SAVE:
			be_paletteClose();
			ok = be_saveFile(&outlen);
			// Do not save when on homepage
			if (state.mode == SPLASH) {
				snprintf(state.statusmsg, sizeof(state.statusmsg), "{X} Action not allowed on homepage");
				state.statusmsg_time = time(NULL);
				return;
			}
			if (ok) {
				snprintf(msg, sizeof(msg), "%.20s saved to disk (%zu bytes).",
						 state.filename, outlen);
			} else {
				snprintf(msg, sizeof(msg), "I/O Error. Could not save file to disk");
			}

			snprintf(state.statusmsg, sizeof(state.statusmsg), "%s", msg);
			state.statusmsg_time = time(NULL);
			return;

		case CMD_SAVEAS:
			be_paletteClose();
			// Do not save when on homepage
			if (state.mode == SPLASH) {
				snprintf(state.statusmsg, sizeof(state.statusmsg), "{X} Action not allowed on homepage");
				state.statusmsg_time = time(NULL);
				return;
			}
			be_saveDialog_open(&state.save_dialog);
			return;

		case CMD_OPEN:
			be_paletteClose();
			// Open File Dialog;
            be_openDialog_open(&state.open_dialog);
			return;
		
		case CMD_QUIT:
			be_paletteClose();
			be_quitNow("Quitting ...");
			break;
		
		case CMD_GOTO:
			be_paletteClose();
			// Do not execute when on homepage
			if (state.mode == SPLASH) {
				snprintf(state.statusmsg, sizeof(state.statusmsg), "{X} Action not allowed on homepage");
				state.statusmsg_time = time(NULL);
				return;
			}

			be_gotoDialog_open(&state.goto_dialog);
			return;

		case CMD_GTST:
			be_paletteClose();
			// Do not execute when on homepage
			if (state.mode == SPLASH) {
				snprintf(state.statusmsg, sizeof(state.statusmsg), "{X} Action not allowed on homepage");
				state.statusmsg_time = time(NULL);
				return;
			}
			state.cur_x = 0;
			state.cur_y = state.def_y;
			return;

		case CMD_GTEN:
			be_paletteClose();
			// Do not execute when on homepage
			if (state.mode == SPLASH) {
				snprintf(state.statusmsg, sizeof(state.statusmsg), "{X} Action not allowed on homepage");
				state.statusmsg_time = time(NULL);
				return;
			}
			state.cur_y = state.numrows ? state.numrows - 1 : 0;
			state.cur_x = state.numrows ? state.row[state.cur_y].size : 0;
			return;

		case CMD_FIND:
			be_paletteClose();
			be_setStatusMsg("{X} Find is not implemented yet");
			return;
	}
}

/* Helper funcitons */
int hex2int(char hex) {
	if (hex >= '0' && hex <= '9') return hex - '0';
	switch (hex) {
		case 'a': case 'A': return 10;
		case 'b': case 'B': return 11;
		case 'c': case 'C': return 12;
		case 'd': case 'D': return 13;
		case 'e': case 'E': return 14;
		case 'f': case 'F': return 15;
	}
	return 0;
}

void hex2rgb(int *r, int *g, int *b, char *hexcode) {
	if (strlen(hexcode) != 7 || hexcode[0] != '#') {
		*r = -1; *g = -1; *b = -1;
		return;
	}

	char *res = hexcode + 1;
	*r = (hex2int(res[0]) * 16) + hex2int(res[1]);
	*g = (hex2int(res[2]) * 16) + hex2int(res[3]);
	*b = (hex2int(res[4]) * 16) + hex2int(res[5]);
}

/* Terminal */
void be_check_and_raise(bool expr, const char *err_msg, BE_ErrorKind err_kind) {
	char *err_head;
	switch (err_kind) {
		case BE_ERR_ASSERT:
			err_head = "Assertion Error";
			break;
		case BE_ERR_RENDER:
			err_head = "Rendering Error";
			break;
		case BE_ERR_TERM:
			err_head = "TerminalOp Error";
			break;
		case BE_ERR_OP:
			err_head = "Operational Error";
			break;
		default:
			err_head = "Unknown Error";
			break;
	}
	if (!expr) {
		fprintf(stderr, "%s: %s\n", err_head, err_msg);
		exit(1);
	}
}

void be_die(const char *s) {
	// int res = write(STDOUT_FILENO, "\x1b[2J", 4);
	// res += write(STDOUT_FILENO, "\x1b[H", 3);
	// be_check_and_raise(res != 7, "Could not clear screen for be_die function", BE_ERR_OP);

    perror(s);
    exit(1);
}

/* NOTE this still is not working perfectly. This needs to be checked and fixed for tmux */
static void be_writeTermSeq(const char *seq, size_t len) {
    if (getenv("TMUX") == NULL) {
        if (write(STDOUT_FILENO, seq, len) != (ssize_t)len) { /* best effort */ }
        return;
    }

    char buf[64];
    size_t n = 0;
    const char *prefix = "\x1bPtmux;";
    for (const char *p = prefix; *p && n < sizeof(buf); p++) buf[n++] = *p;
    for (size_t i = 0; i < len && n + 2 <= sizeof(buf); i++) {
        if (seq[i] == '\x1b' && n < sizeof(buf)) buf[n++] = '\x1b';
        buf[n++] = seq[i];
    }
    const char *suffix = "\x1b\\";
    for (const char *p = suffix; *p && n < sizeof(buf); p++) buf[n++] = *p;

    if (write(STDOUT_FILENO, buf, n) != (ssize_t)n) { /* best effort */ }
}

void be_disableRawMode(void) {
    /* Pop the keyboard protocol enhancement pushed in be_enableRawMode():
     * leaving it on would change how keys are reported for whatever the
     * user's shell runs next. Terminals that never understood the push
     * ignore the pop the same harmless way. */
    be_writeTermSeq("\x1b[<u", 4);
    /* Cursor visibility is terminal-global and outlives the process: if we
     * exit while hidden, the user's shell inherits an invisible cursor. */
    if (write(STDOUT_FILENO, "\x1b[?25h", 6) != 6) { /* best effort */ }
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.orig_termios) == -1)
        be_die("tcsetattr");
}


void be_enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &state.orig_termios) == -1) be_die("tcgetattr");
    atexit(be_disableRawMode);

    struct termios raw = state.orig_termios;
    // Set raw mode by disabling echo and canonical mode
    // raw.c_iflag = ~(IXON | ICRNL);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) be_die("tcsetattr");

    /* Push the Kitty keyboard protocol's "disambiguate escape codes" flag
     * (bit 1). Without it, Ctrl+Shift+S is the exact same byte as Ctrl+S on
     * the wire — there's no way to tell them apart. Terminals that support
     * the protocol start reporting Ctrl+Shift+<letter> (and a few other
     * otherwise-ambiguous keys) as distinct CSI sequences; terminals that
     * don't recognize this escape simply ignore it and nothing changes. */
    be_writeTermSeq("\x1b[>1u", 5);
}

int be_readKey() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) be_die("read");
	}

	if (c != '\x1b') {
	    return (unsigned char)c;
	}

	char ch;
	if (read(STDIN_FILENO, &ch, 1) != 1) return '\x1b';

	/* SS3 form: ESC O <final>. Never carries modifiers. */
    if (ch == 'O') {
            if (read(STDIN_FILENO, &ch, 1) != 1) return '\x1b';
            switch (ch) {
            case 'H': return HOME;
            case 'F': return END;
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            }
            return '\x1b';
    }

    /* Bare "ESC <byte>" form: Meta/Alt-modified keys. Terminals encode
     * Alt+<key> this way by default (no CSI wrapper) rather than as a
     * modifier bit inside a CSI sequence — that's why Alt+b / Alt+f never
     * matched anything below. Ctrl+Backspace is indistinguishable from
     * Alt+Backspace at the protocol level too: both arrive as ESC
     * followed by DEL (0x7f), so both are handled here. */
    if (ch != '[') {
        switch (ch) {
            case 'b': return WORD_LEFT;
            case 'f': return WORD_RIGHT;
            case 0x7f:
            case 0x08: return CTRL_BACKSPACE;
            case 'a':  return ALT_A;
            case 'e':  return ALT_E;
        }
        return '\x1b';
    }

    /* CSI form: collect params until a final byte (0x40-0x7E). */
    char seq[24];
    size_t n = 0;
    char final = 0;

    while (n < sizeof(seq) - 1) {
        if (read(STDIN_FILENO, &ch, 1) != 1) return '\x1b';
        if (ch >= 0x40 && ch <= 0x7E) {
            final = ch;
            break;
        }
        seq[n++] = ch;
    }
    seq[n] = '\0';
    if (final == 0) return '\x1b';

    int p1 = 1, p2 = 1;
    sscanf(seq, "%d;%d", &p1, &p2);
    int mods = p2-1;

    switch (final) {
        case 'H': return (mods & MOD_CTRL) ? CTRL_HOME : HOME;
        case 'F': return (mods & MOD_CTRL) ? CTRL_END : END;
		case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
		case 'C': return (mods & MOD_CTRL) ? WORD_RIGHT : ARROW_RIGHT;
		case 'D': return (mods & MOD_CTRL) ? WORD_LEFT : ARROW_LEFT;

        case '~':
            switch (p1) {
                case 1: case 7: return (mods & MOD_CTRL) ? CTRL_HOME : HOME;
                case 4: case 8: return (mods & MOD_CTRL) ? CTRL_END : END;
				case 3: return (mods & MOD_CTRL) ? WORD_DELETE : DELETE;
                case 5: return PAGE_UP;
                case 6: return PAGE_DOWN;
            }
            break;

        case '^':
            switch (p1) {
                case 7: return CTRL_HOME;
                case 8: return CTRL_END;
            }
            break;

		// Kitty keyboard protocol ("disambiguate escape codes"), which
		// be_enableRawMode() opts into: "CSI <codepoint> ; <mods> u". Once a
		// terminal supports it, this isn't just how Ctrl+Shift+S shows up —
		// Esc/Enter/Tab/Backspace and *every* Ctrl-modified key switch to
		// this form too, since that's what makes Ctrl+Shift+<letter>
		// representable at all (on an unmodified terminal it's the exact
		// same byte as plain Ctrl+<letter>). So this has to reconstruct
		// every one of those, not just special-case the one combo it was
		// added for — otherwise every existing Ctrl+ binding breaks on
		// terminals that do support the protocol. Terminals that don't
		// support it never send 'u' sequences, so none of this ever runs
		// for them and nothing changes.
		case 'u':
			if (p1 == 27)  return '\x1b';
			if (p1 == 13)  return '\r';
			if (p1 == 9)   return '\t';
			if (p1 == 127) return (mods & (MOD_CTRL | MOD_ALT)) ? CTRL_BACKSPACE : BACKSPACE;

			if ((p1 == 's' || p1 == 'S') && (mods & MOD_CTRL) && (mods & MOD_SHIFT))
				return CTRL_SHIFT_S;

			// Plain Ctrl+<letter> (no Shift): collapse back to the legacy
			// control byte CTRL_KEY() produces, so every other Ctrl+
			// binding in the app keeps working unchanged.
			if ((mods & MOD_CTRL) && !(mods & MOD_SHIFT) &&
			    ((p1 >= 'a' && p1 <= 'z') || (p1 >= 'A' && p1 <= 'Z')))
				return CTRL_KEY(p1);
			break;
    }
    return '\x1b';
}

int be_getCursorPos(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
	return 0;
}

void be_moveCursor(StringBuilder *sb, int x, int y) {
        if (x >= state.screencols) x = state.screencols - 1;
        if (y >= state.screenrows) y = state.screenrows;

        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, x + 1);
        sb_append(sb, buf);
}

int be_getWindowSize(int *rows, int *cols) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return be_getCursorPos(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}


/* Rendering */
/* Display width of a UTF-8 string, in terminal cells. */
static int utf8_width(const char *s) {
	int w = 0;
	while (*s) {
		if (((unsigned char)*s & 0xC0) != 0x80) w++;  /* skip continuation bytes */
		s++;
	}
	return w;
}

static int line_width(const BE_HomepageLine *ln) {
	int w = 0;
	for (int i = 0; i < ln->nspans; i++)
		w += utf8_width(ln->spans[i].text);
	return w;
}

int be_drawRows(StringBuilder *sb) {
	int y, res = 1;
	for (y = 0; y < state.screenrows; y++) {
		int filerow = y + state.rowoff;
		if (filerow >= state.numrows) {
			if (y < state.screenrows - 1) {
				sb_append(sb, "~");
			}
		}
		else {
			// Determine line length (bounded to the space left after the gutter)
			int avail = state.screencols - state.def_x;
			if (avail < 0) avail = 0;
			int len = state.row[filerow].rsize - state.coloff;
			if (len < 0) len = 0;
			if (len > avail) len = avail;
			// Append line-no to the line
			char linenum[64];
			snprintf(linenum, sizeof(linenum), "%s%5d│ %s", "\x1b["S_DIM"m", filerow+1, "\x1b[0m");
			sb_append(sb, linenum);

			if (len > 0) {
				sb_appendn(sb, &state.row[filerow].render[state.coloff], (size_t)len);
			}
			res = 0;
		}
		sb_append(sb, "\x1b[K");
		sb_append(sb, "\r\n");
	}

	return res;
}

void be_drawStatusBar(StringBuilder *sb) {
	// Move the cursor to the right position (END)
	be_moveCursor(sb, 0, state.screenrows);

	// Prepare the foreground and background sequences
	int br, bg, bb;
	int fr, fg, fb;
	char backbuf[32];
	char forebuf[32];
	hex2rgb(&br, &bg, &bb, sb_theme.sb_background);
	hex2rgb(&fr, &fg, &fb, sb_theme.sb_foreground);

	snprintf(backbuf, sizeof(backbuf), "\033[48;2;%d;%d;%dm", br, bg, bb);
	snprintf(forebuf, sizeof(forebuf), "\033[38;2;%d;%d;%dm", fr, fg, fb);

	sb_append(sb, backbuf); // set the background
	sb_append(sb, forebuf); // set the foreground
    
    int cols = state.screencols;
    if (cols < 0) cols = 0;
    if (cols > 511) cols = 511;

	// Define the left, right and center status buffers
	char lstatus[80], rstatus[120], cstatus[80];

    // Render left status into the left buffer
    int llen = snprintf(lstatus, sizeof(lstatus), "   ✽ | %.20s | [%d:%d|%d] %s", 
                        state.filename ? state.filename : "[No Name]", 
                        state.cur_y+1, state.cur_x + 1, state.numrows,
                        state.dirty ? "*" : "");
    if (llen < 0) llen = 0;
    if (llen > cols) llen = cols;

    // Render right status into the right buffer
    int rlen = snprintf(rstatus, sizeof(rstatus), "^ S Save | ^ Q Quit | ^ P Cmd Pallete");
    if (rlen < 0) rlen = 0;
    if (rlen > (int)sizeof(rstatus) - 1) rlen = sizeof(rstatus) - 1;

    int show_msg = difftime(time(NULL), state.statusmsg_time) <= 2.0;
    int clen = 0;
    if (show_msg) {
        clen = snprintf(cstatus, sizeof(cstatus), "%s", state.statusmsg);
        if (clen < 0) clen = 0;
        if (clen > (int)sizeof(cstatus) - 1) clen = sizeof(cstatus) - 1;
    }

    // Compose the status bar row into one buffer    
    char line[512];
    memset(line, ' ', cols);
    line[cols] = '\0';

    // Left status
    memcpy(line, lstatus, llen);

    // Center status
    if (clen > 0) {
        int cstart = (cols - clen) / 2;
        if (cstart < llen) cstart = llen;   // never overwrite the left status
        if (cstart + clen <= cols) memcpy(line + cstart, cstatus, clen);
    }
    
    // Right status
    if (rlen > 0 && cols - rlen >= llen) {
        int rstart = cols - rlen;
        int center_end = clen > 0 ? (cols - clen) / 2 + clen : llen;
        if (center_end < llen) center_end = llen;
        if (rstart >= center_end) memcpy(line + rstart, rstatus, rlen);
    }

    sb_append(sb, line);

    // Reset & Move the cursor back to start
    sb_append(sb, "\x1b[K");
    sb_append(sb, "\x1b[0m");
    be_moveCursor(sb, state.def_x, state.def_y);

}

void be_drawHomepage(StringBuilder *sb) {
	int n = (int)(sizeof(homepage) / sizeof(homepage[0]));

	int starty = (state.screenrows - n) / 2;
	if (starty < 0 || state.screencols < 50) return;   /* too small; bail */

	/* Pass 1: widest AL_BLOCK line decides the shared left edge. */
	int block_w = 0;
	for (int i = 0; i < n; i++) {

		if (homepage[i].align != AL_BLOCK) continue;
		int w = line_width(&homepage[i]);
		if (w > block_w) block_w = w;
	}
	int block_x = (state.screencols - block_w) / 2;

	/* Pass 2: position and emit. */
	for (int i = 0; i < n; i++) {
		const BE_HomepageLine *ln = &homepage[i];
		int x = (ln->align == AL_BLOCK) ? block_x : (state.screencols - line_width(ln)) / 2;

		if (x < 0) x = 0;
		be_moveCursor(sb, x, starty + i);

		for (int j = 0; j < ln->nspans; j++) {
			const BE_Span *sp = &ln->spans[j];
			if (sp->style) {
				sb_append(sb, "\x1b[");
				sb_append(sb, sp->style);
				sb_append(sb, "m");
			}
			sb_append(sb, sp->text);
			if (sp->style) sb_append(sb, "\x1b[0m");
		}
	}

}

/* Save Dialog Drawing Helpers */
/* An empty row: just the two side borders with the interior blanked. */
static void be_drawDialogPadRow(StringBuilder *sb, int x, int y, int inner) {
	be_moveCursor(sb, x, y);
	sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
	for (int i = 0; i < inner; i++) sb_append(sb, " ");
	sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
}

/* A row with a single left-aligned styled message, padded out to the
 * right border. */
static void be_drawDialogTextRow(StringBuilder *sb, int x, int y, int inner,
                                  const char *style, const char *text) {
	be_moveCursor(sb, x, y);
	sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
	sb_append(sb, "\x1b[");
	sb_append(sb, style);
	sb_append(sb, "m");
	sb_append(sb, text);
	sb_append(sb, S_RESET);
	for (int i = 0; i < inner - utf8_width(text); i++) sb_append(sb, " ");
	sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
}

/* One button in the button bar: highlighted (reverse video) when it has
 * focus, dim otherwise. Returns the number of cells it drew, so the
 * caller can work out how much trailing padding is left. */
static int be_drawDialogButton(StringBuilder *sb, const char *label, bool focused) {
	sb_append(sb, "  ");
	sb_append(sb, focused ? "\x1b[7;" S_ACCENT "m" : "\x1b[" S_DIM "m");
	sb_append(sb, label);
	sb_append(sb, S_RESET);
	return 2 + utf8_width(label);
}

void be_drawSaveDialog(StringBuilder *sb, int *cur_x, int *cur_y) {
    BE_SaveDialog *sd = &state.save_dialog;

    /* Geometry */
    int width = state.screencols / 4;
    int height = 10;
    int x = (state.screencols - width) / 2;
    int y = (state.screenrows - height) / 2;
    int inner = width - 2;

    /* Top Border */
    be_moveCursor(sb, x, y);
    sb_append(sb, "\x1b[" S_DIM "m" B_TL);
    sb_append(sb, B_H " *unsaved changes ");
    for (int i = 0; i < width - 21; i++) sb_append(sb, B_H);
    sb_append(sb, B_TR S_RESET);

	/* First Pad */
    be_drawDialogPadRow(sb, x, y + 1, inner);

	/* Save Dailog Message */
	char buf[80];
	const char *file_desc = (state.filename == NULL) ? "File" : state.filename;
	snprintf(buf, sizeof(buf), "  %s has been modified.", file_desc);

    be_drawDialogTextRow(sb, x, y + 2, inner, S_TEXT, buf);
    be_drawDialogTextRow(sb, x, y + 3, inner, S_TEXT, "  Save before quitting ?");

	/* Second Pad */
    be_drawDialogPadRow(sb, x, y + 4, inner);

	/* Text Box */
    be_drawDialogTextRow(sb, x, y + 5, inner, S_DIM, "  save to");

    // Text Box: keep the caret close to the border so typing starts right
    // where you're looking, instead of several cells in.
    be_moveCursor(sb, x, y + 6);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
    sb_append(sb, "\x1b[" S_ACCENT "m" " [ " S_RESET);
    sb_append(sb, "\x1b[" S_TEXT "m");
    sb_append(sb, sd->input_path);
    int box_used = 3 + (int)sd->inputlen; // " [ " + typed text
    for (int i = 0; i < inner - box_used - 3; i++) sb_append(sb, " "); // leave room for " ] "
    sb_append(sb, S_RESET "\x1b[" S_ACCENT "m" " ] " S_RESET);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

	/* Third Pad */
    be_drawDialogPadRow(sb, x, y + 7, inner);

    // Buttons: exactly one of Save/Quit/Cancel is focused at a time (Tab
    // or Left/Right cycles focus; Enter runs whichever is focused). Their
    // hotkeys (s/q/c) still work no matter which button has focus.
    be_moveCursor(sb, x, y + 8);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

    const char *labels[3] = { " (S)ave ", " (Q)uit ", " (C)ancel " };
    int used = 0;
    for (int i = 0; i < 3; i++) {
        bool focused = ((int)sd->focus == SD_FOCUS_SAVE + i);
        used += be_drawDialogButton(sb, labels[i], focused);
    }
    for (int i = 0; i < inner - used; i++) sb_append(sb, " ");
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

	/* Last Pad */
    be_drawDialogPadRow(sb, x, y + 9, inner);

    /* Bottom border */
    be_moveCursor(sb, x, y + 10);
    sb_append(sb, "\x1b[" S_DIM "m" B_BL B_H);
    const char *btm_msg = "tab/←→ switch · enter select · esc cancel";
    sb_append(sb, btm_msg);
    for (int i = 0; i < inner - utf8_width(btm_msg) - 1; i++) sb_append(sb, B_H);
    sb_append(sb, B_BR S_RESET);

	// Land the caret right after the typed text inside the box: border(1)
	// + " [ "(3) = 4 cells in, then past whatever's typed so far.
	*cur_x = x + 4 + (int)sd->inputlen;
	*cur_y = y + 6;
}

void be_drawOpenDialog(StringBuilder *sb, int *cur_x, int *cur_y) {
	BE_OpenDialog *od = &state.open_dialog;

    /* Geometry */
    int width = state.screencols / 4;
    int height = 10;
    int x = (state.screencols - width) / 2;
    int y = (state.screenrows - height) / 2;
    int inner = width - 2;

    /* Top Border */
    be_moveCursor(sb, x, y);
    sb_append(sb, "\x1b[" S_DIM "m" B_TL);
    sb_append(sb, B_H " Open File ");
    for (int i = 0; i < width - 14; i++) sb_append(sb, B_H);
    sb_append(sb, B_TR S_RESET);

	/* First Pad */
    be_drawDialogPadRow(sb, x, y + 1, inner);

	/* Open Dailog Message */
	char buf[80];
	snprintf(buf, sizeof(buf), "  Open a file:");
    be_drawDialogTextRow(sb, x, y + 2, inner, S_TEXT, buf);

	/* Second Pad */
    be_drawDialogPadRow(sb, x, y + 3, inner);

	/* Text Box */
    be_drawDialogTextRow(sb, x, y + 4, inner, S_DIM, "  open file:");

    be_moveCursor(sb, x, y + 5);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
    sb_append(sb, "\x1b[" S_ACCENT "m" " [ " S_RESET);
    sb_append(sb, "\x1b[" S_TEXT "m");
    sb_append(sb, od->input_path);
    int box_used = 3 + (int)od->inputlen;
    for (int i = 0; i < inner - box_used - 3; i++) sb_append(sb, " ");
    sb_append(sb, S_RESET "\x1b[" S_ACCENT "m" " ] " S_RESET);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

	/* Third Pad */
    be_drawDialogPadRow(sb, x, y + 6, inner);

    be_moveCursor(sb, x, y + 7);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

    const char *labels[2] = { " (O)pen ", " (C)ancel " };
    int used = 0;
    for (int i = 0; i < 2; i++) {
        bool focused = ((int)od->focus == OD_FOCUS_OPEN + i);
        used += be_drawDialogButton(sb, labels[i], focused);
    }
    for (int i = 0; i < inner - used; i++) sb_append(sb, " ");
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

	/* Last Pad */
    be_drawDialogPadRow(sb, x, y + 8, inner);

    /* Bottom border */
    be_moveCursor(sb, x, y + 9);
    sb_append(sb, "\x1b[" S_DIM "m" B_BL B_H);
    const char *btm_msg = "tab/←→ switch · enter select · esc cancel";
    sb_append(sb, btm_msg);
    for (int i = 0; i < inner - utf8_width(btm_msg) - 1; i++) sb_append(sb, B_H);
    sb_append(sb, B_BR S_RESET);

	*cur_x = x + 4 + (int)od->inputlen;
	*cur_y = y + 5;
}

void be_drawGotoDialog(StringBuilder *sb, int *cur_x, int *cur_y) {
	BE_GotoDialog *gd = &state.goto_dialog;

    /* Geometry */
    int width = state.screencols / 4;
    int height = 10;
    int x = (state.screencols - width) / 2;
    int y = (state.screenrows - height) / 2;
    int inner = width - 2;

    /* Top Border */
    be_moveCursor(sb, x, y);
    sb_append(sb, "\x1b[" S_DIM "m" B_TL);
    sb_append(sb, B_H " Open File ");
    for (int i = 0; i < width - 14; i++) sb_append(sb, B_H);
    sb_append(sb, B_TR S_RESET);

	/* First Pad */
    be_drawDialogPadRow(sb, x, y + 1, inner);

	/* Open Dailog Message */
	char buf[80];
	snprintf(buf, sizeof(buf), "  Goto a line:");
    be_drawDialogTextRow(sb, x, y + 2, inner, S_TEXT, buf);

	/* Second Pad */
    be_drawDialogPadRow(sb, x, y + 3, inner);

	/* Text Box */
    be_drawDialogTextRow(sb, x, y + 4, inner, S_DIM, "  goto: ");

    be_moveCursor(sb, x, y + 5);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);
    sb_append(sb, "\x1b[" S_ACCENT "m" " [ " S_RESET);
    sb_append(sb, "\x1b[" S_TEXT "m");
    sb_append(sb, gd->input);
    int box_used = 3 + (int)gd->inputlen;
    for (int i = 0; i < inner - box_used - 3; i++) sb_append(sb, " ");
    sb_append(sb, S_RESET "\x1b[" S_ACCENT "m" " ] " S_RESET);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

	/* Third Pad */
    be_drawDialogPadRow(sb, x, y + 6, inner);

    be_moveCursor(sb, x, y + 7);
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

    const char *labels[2] = { " (G)oto ", " (C)ancel " };
    int used = 0;
    for (int i = 0; i < 2; i++) {
        bool focused = ((int)gd->focus == GD_FOCUS_GOTO + i);
        used += be_drawDialogButton(sb, labels[i], focused);
    }
    for (int i = 0; i < inner - used; i++) sb_append(sb, " ");
    sb_append(sb, "\x1b[" S_DIM "m" B_V S_RESET);

	/* Last Pad */
    be_drawDialogPadRow(sb, x, y + 8, inner);

    /* Bottom border */
    be_moveCursor(sb, x, y + 9);
    sb_append(sb, "\x1b[" S_DIM "m" B_BL B_H);
    const char *btm_msg = "tab/←→ switch · enter select · esc cancel";
    sb_append(sb, btm_msg);
    for (int i = 0; i < inner - utf8_width(btm_msg) - 1; i++) sb_append(sb, B_H);
    sb_append(sb, B_BR S_RESET);

	*cur_x = x + 4 + (int)gd->inputlen;
	*cur_y = y + 5;
}

void be_drawPalette(StringBuilder *sb, int *cur_x, int *cur_y) {
	BE_CmdPalette *p = &state.pal;

	/* Geometry */
	int width = state.screencols - 8;
	if (width > 56) width = 56;
	if (width < 18 || state.screenrows < 12) return;

	int inner = width - 2;

	int nvis = p->nfiltered;
	if (nvis > p->max_rows || nvis < 1) nvis = p->max_rows;
	int listrows = (p->nfiltered == 0) ? 1 : nvis;

	int x = (state.screencols - width) / 2;
	int y = 2;

	/* scroll the list so the selection stays visible */
	int top = 0;
	if (p->sel >= p->max_rows) top = p->sel - p->max_rows + 1;

	/* Top Border */
	be_moveCursor(sb, x, y);
	sb_append(sb, PAL_DIM B_TL);
	sb_repeat(sb, B_H, inner);
	sb_append(sb, B_TR PAL_RESET);

	/* Input Field */
	int qshow = p->qlen;
	if (qshow > inner - 3) qshow = inner - 3;

	be_moveCursor(sb, x, y+1);
	sb_append(sb, PAL_DIM B_V PAL_RESET);
	sb_append(sb, " " PAL_ACCENT "\xe2\x80\xba " PAL_RESET);
	sb_append(sb, PAL_BOLD);
	for (int i = 0; i < qshow; i++) {
		char ch[2] = { p->query[i], '\0' };
		sb_append(sb, ch);
	}
	
	sb_append(sb, PAL_RESET);
	sb_repeat(sb, " ", inner - 3 - qshow);
	sb_append(sb, PAL_DIM B_V PAL_RESET);

	/* Separator */
	be_moveCursor(sb, x, y+2);
	sb_append(sb, PAL_DIM B_LT);
	sb_repeat(sb, B_H, inner);
	sb_append(sb, B_RT PAL_RESET);

	*cur_x = x + 4 + qshow;
	*cur_y = y + 1;

	/* Rows */
	for (int r = 0; r < listrows; r++) {
		int row_y = y + 3 + r;
		be_moveCursor(sb, x, row_y);
		sb_append(sb, PAL_DIM B_V PAL_RESET);

		if (p->nfiltered == 0) {
			const char *msg = PAL_DIM "No matching commands found" PAL_RESET;
			sb_append(sb, msg);
			sb_repeat(sb, " ", inner - 26);
			sb_append(sb, PAL_DIM B_V PAL_RESET);
			continue;
		}

		int idx = p->filtered[top+r];
		bool selected = (top + r == p->sel);
		const char *name = commands[idx].name;
		const char *hint = commands[idx].hint;

		int namew = utf8_width(name);
		int hintw = utf8_width(hint);

		if (namew > inner - 4) namew = inner - 4;
		int gap = inner - 2 - namew - hintw;
		if (gap < 1) { 
			hintw = 0; 
			hint = ""; 
			gap = inner - 2 - namew; 
		}

		if (selected) sb_append(sb, PAL_SEL);

		sb_append(sb, " ");
		sb_append(sb, selected ? PAL_ACCENT : PAL_TEXT);
		if (selected) sb_append(sb, PAL_SEL);
		for (int i = 0; i < namew; i++) {
			char ch[2] = {name[i], '\0' };
			sb_append(sb, ch);
		}
		sb_append(sb, PAL_RESET);
		if (selected) sb_append(sb, PAL_SEL);
		sb_repeat(sb, " ", gap);
		sb_append(sb, PAL_DIM);
		if (selected) sb_append(sb, PAL_SEL);

		sb_append(sb, hint);
		sb_append(sb, " ");
		sb_append(sb, PAL_RESET);

		sb_append(sb, B_V);
	}

	/* Bottom Border */
	be_moveCursor(sb, x, y + listrows + 3);
	sb_append(sb, PAL_DIM B_BL);
	sb_repeat(sb, B_H, inner);
	sb_append(sb, B_BR PAL_RESET);
}

/* line editing */
int is_word_char(char c) { return !isspace((unsigned char)c); }

int word_backward_pos(const BE_Row *row) {
	int pos = state.cur_x;
	while (pos > 0 && !is_word_char(row->data[pos-1])) pos--;
	while (pos > 0 && is_word_char(row->data[pos-1])) pos--;
	return pos;
}

int word_forward_pos(const BE_Row *row) {
	int pos = state.cur_x;
	while (pos < row->size && !is_word_char(row->data[pos])) pos++;
	while (pos < row->size && is_word_char(row->data[pos])) pos++;
	return pos;
}

void be_word_move_forward() {
	if (state.cur_y < 0 || state.cur_y >= state.numrows) return;
	BE_Row *row = &state.row[state.cur_y];
	state.cur_x = word_forward_pos(row);
}

void be_word_move_backward() {
	if (state.cur_y < 0 || state.cur_y >= state.numrows) return;
	BE_Row *row = &state.row[state.cur_y];
	state.cur_x = word_backward_pos(row);
}

void be_delete_word_backward() {
	if (state.cur_y < 0 || state.cur_y >= state.numrows) return;
	BE_Row *row = &state.row[state.cur_y];
	int start = word_backward_pos(row);
	int end = state.cur_x;
	if (start == end) return;
	memmove(&row->data[start], &row->data[end], row->size - end);
	row->size -= (end - start);
	row->data[row->size] = '\0';
	state.cur_x = start;
	be_updateRow(row);
	state.dirty++;
}

void be_delete_word_forward() {
	if (state.cur_y < 0 || state.cur_y >= state.numrows) return;
	BE_Row *row = &state.row[state.cur_y];
	int start = state.cur_x;
	int end = word_forward_pos(row);
	if (start == end) return;
	memmove(&row->data[start], &row->data[end], row->size - end);
	row->size -= (end - start);
	row->data[row->size] = '\0';
	be_updateRow(row);
	state.dirty++;
}

void line_delete_to_start() {
	if (state.cur_y < 0 || state.cur_y >= state.numrows) return;
	BE_Row *row = &state.row[state.cur_y];

    memmove(&row->data[0], &row->data[state.cur_x], row->size - state.cur_x);
    row->size -= state.cur_x;
    row->data[row->size] = '\0';
    state.cur_x = 0;
    be_updateRow(row);
    state.dirty++;
}

void line_delete_to_end() {
	if (state.cur_y < 0 || state.cur_y >= state.numrows) return;
	BE_Row *row = &state.row[state.cur_y];

	row->size = state.cur_x;
	row->data[row->size] = '\0';
	be_updateRow(row);
	state.dirty++;
}


/* Input Processing */
void be_handleArrowKeys(int key) {
	int lastrow = state.numrows ? state.numrows - 1 : 0;
	BE_Row *row = (state.cur_y >= state.numrows) ? NULL : &state.row[state.cur_y];

	switch (key) {
		case ARROW_LEFT:
			if (state.cur_x != 0) {
				state.cur_x--;
			} else if (state.cur_y > 0){
				state.cur_y--;
				state.cur_x = state.row[state.cur_y].size;
			}
			break;
		case ARROW_RIGHT:
			if (row && state.cur_x < row->size) {
				state.cur_x++;
			} else if (row && state.cur_x == row->size && state.cur_y < lastrow) {
				state.cur_y++;
				state.cur_x = 0;
			}
			break;
		case ARROW_UP:
			if (state.cur_y != 0) state.cur_y--;
			break;
		case ARROW_DOWN:
			if (state.cur_y != state.numrows) state.cur_y++;
			break;
	}

	row = (state.cur_y >= state.numrows) ? NULL : &state.row[state.cur_y];
	int rowlen = row ? row->size : 0;
	if (state.cur_x > rowlen) {
		state.cur_x = rowlen;
	}
}


static void be_saveDialog_commit(void) {
	BE_SaveDialog *sd = &state.save_dialog;

	if (state.filename != NULL) free(state.filename);
	state.filename = strdup(sd->input_path);

	size_t saved_len = 0;
	bool ok = be_saveFile(&saved_len);

	if (sd->quit_on_save) {
		char msg[96];
		if (ok) {
			snprintf(msg, sizeof(msg), "%.20s saved to disk (%zu bytes). ✓ Quitting ...",
			         state.filename, saved_len);
		} else {
			snprintf(msg, sizeof(msg), "%s", state.statusmsg); // be_saveFile's I/O error
		}
		be_quitNow(msg);
		return;
	}
	be_saveDialog_close(sd);
}

void processSaveDialogKeypress(int c) {
	BE_SaveDialog *sd = &state.save_dialog;

	switch (c) {
		case '\x1b':
			be_saveDialog_close(sd);
			return;

		// Tab always cycles focus forward: input -> Save -> Quit -> Cancel -> input.
		case '\t':
			sd->focus = (sd->focus + 1) % SD_FOCUS_COUNT;
			return;

		// Left/Right also walk the button bar, once a button has focus.
		case ARROW_LEFT:
		case ARROW_RIGHT:
			if (sd->focus != SD_FOCUS_INPUT) {
				int btn = sd->focus - SD_FOCUS_SAVE;
				int dir = (c == ARROW_RIGHT) ? 1 : -1;
				btn = (btn + dir + 3) % 3;
				sd->focus = SD_FOCUS_SAVE + btn;
			}
			return;

		case BACKSPACE:
		case CTRL_KEY('h'):
			if (sd->focus == SD_FOCUS_INPUT && sd->inputlen > 0) {
				sd->input_path[--sd->inputlen] = '\0';
			}
			return;

		// Enter performs whatever currently has focus: from the input bar
		// that's Save (finish typing, then commit), from a button it's
		// that button's action.
		case '\r':
			switch (sd->focus) {
				case SD_FOCUS_INPUT:
				case SD_FOCUS_SAVE:
					be_saveDialog_commit();
					return;
				case SD_FOCUS_QUIT:
					be_quitNow(NULL);
					return;
				case SD_FOCUS_CANCEL:
				default:
					be_saveDialog_close(sd);
					return;
			}
	}

	if (sd->focus == SD_FOCUS_INPUT) {
		if (c >= 32 && c < 127 && sd->inputlen < MAX_INPUT_SIZE) {
			sd->input_path[sd->inputlen++] = (char)c;
			sd->input_path[sd->inputlen] = '\0';
		}
		return;
	}

	// Direct hotkeys still work no matter which button currently has focus.
	switch (c) {
		case 'c':
			be_saveDialog_close(sd);
			return;
		case 'q':
			be_quitNow(NULL);
			return;
		case 's':
			be_saveDialog_commit();
			return;
	}
}


void processOpenDialogKeypress(int c) {
	BE_OpenDialog *od = &state.open_dialog;

	switch (c) {
		case '\x1b':
			be_openDialog_close(od);
			return;

		// Tab always cycles focus forward: input -> Save -> Quit -> Cancel -> input.
		case '\t':
			od->focus = (od->focus + 1) % OD_FOCUS_COUNT;
			return;

		// Left/Right also walk the button bar, once a button has focus.
		case ARROW_LEFT:
		case ARROW_RIGHT:
			if (od->focus != OD_FOCUS_INPUT) {
				int btn = od->focus - OD_FOCUS_OPEN;
				int dir = (c == ARROW_RIGHT) ? 1 : -1;
				btn = (btn + dir + 2) % 2;
				od->focus = OD_FOCUS_OPEN + btn;
			}
			return;

		case BACKSPACE:
		case CTRL_BACKSPACE:
			if (od->focus == OD_FOCUS_INPUT && od->inputlen > 0) {
				od->input_path[--od->inputlen] = '\0';
			}
			return;

		// Enter performs whatever currently has focus: from the input bar
		// that's Save (finish typing, then commit), from a button it's
		// that button's action.
		case '\r':
			switch (od->focus) {
				case OD_FOCUS_INPUT:
				case OD_FOCUS_OPEN:
					be_openDialog_commit();
					return;
				case OD_FOCUS_CANCEL:
				default:
					be_openDialog_close(od);
					return;
			}
	}

	if (od->focus == OD_FOCUS_INPUT) {
		if (c >= 32 && c < 127 && od->inputlen < MAX_INPUT_SIZE) {
			od->input_path[od->inputlen++] = (char)c;
			od->input_path[od->inputlen] = '\0';
		}
		return;
	}

	// Direct hotkeys still work no matter which button currently has focus.
	switch (c) {
		case 'c':
			be_openDialog_close(od);
			return;
		case 'o':
			be_openDialog_commit();
			return;
	}
}

void processGotoDialogKeypress(int c) {
	BE_GotoDialog *gd = &state.goto_dialog;

	switch (c) {
		case '\x1b':
			be_gotoDialog_close(gd);
			return;

		// Tab always cycles focus forward: input -> Save -> Quit -> Cancel -> input.
		case '\t':
			gd->focus = (gd->focus + 1) % OD_FOCUS_COUNT;
			return;

		// Left/Right also walk the button bar, once a button has focus.
		case ARROW_LEFT:
		case ARROW_RIGHT:
			if (gd->focus != GD_FOCUS_INPUT) {
				int btn = gd->focus - GD_FOCUS_GOTO;
				int dir = (c == ARROW_RIGHT) ? 1 : -1;
				btn = (btn + dir + 2) % 2;
				gd->focus = GD_FOCUS_GOTO + btn;
			}
			return;

		case BACKSPACE:
		case CTRL_BACKSPACE:
			if (gd->focus == GD_FOCUS_INPUT && gd->inputlen > 0) {
				gd->input[--gd->inputlen] = '\0';
			}
			return;

		case '\r':
			switch (gd->focus) {
				case GD_FOCUS_INPUT:
				case GD_FOCUS_GOTO:
					be_gotoDialog_commit(&state.goto_dialog);
					return;
				case OD_FOCUS_CANCEL:
				default:
					be_gotoDialog_close(gd);
					return;
			}
	}

	if (gd->focus == GD_FOCUS_INPUT) {
		if (c >= 32 && c < 127 && gd->inputlen < MAX_INPUT_SIZE) {
			gd->input[gd->inputlen++] = (char)c;
			gd->input[gd->inputlen] = '\0';
		}
		return;
	}

	// Direct hotkeys still work no matter which button currently has focus.
	switch (c) {
		case 'c':
			be_gotoDialog_close(gd);
			return;
		case 'g':
			be_gotoDialog_commit(&state.goto_dialog);
			return;
	}

}

void processPaletteKeypress(int c) {
	BE_CmdPalette *p = &state.pal;
	switch (c) {
		case '\x1b':
		case CTRL_KEY('p'):
			be_paletteClose();
			break;

		case '\r':
			be_paletteExecuteCmd();
			return;

		case ARROW_UP:
		case CTRL_KEY('k'):
			if (p->sel > 0) p->sel--;
			return;

		case ARROW_DOWN:
		case CTRL_KEY('j'):
			if (p->sel < p->nfiltered - 1) p->sel++;
			return;


		case BACKSPACE:
		case CTRL_BACKSPACE:
			if (p->qlen > 0) {
				p->query[--p->qlen] = '\0';
				p->sel = 0;
				be_paletteRefilter();
			}
			return;

		case CTRL_KEY('d'):
			p->qlen = 0;
			p->query[0] = '\0';
			p->sel = 0;
			be_paletteRefilter();
			return;
	}

	if (c >= 32 && c < 127 && p->qlen < MAX_INPUT_SIZE) {
		p->query[p->qlen++] = (char)c;
		p->query[p->qlen] = '\0';
		p->sel = 0;
		be_paletteRefilter();
	}
}

void be_processKeypress() {
	int c = be_readKey();

	if (state.mode == SPLASH) {
		if (state.pal.open) {
			processPaletteKeypress(c);
			return;
		}

		if (state.open_dialog.active) {
			processOpenDialogKeypress(c);
			return;
		}

		switch(c) {
			case '\r':
				be_openBlankFile();
				return;

			case CTRL_KEY('q'):
				be_quitEditor();
				return;

			case CTRL_KEY('p'):
				be_paletteOpen();
				return;

			case CTRL_KEY('o'):
				be_openDialog_open(&state.open_dialog);
				return;
		}
		return;
	}

	if (state.save_dialog.active) {
		processSaveDialogKeypress(c);
		return;
	}

	if (state.open_dialog.active) {
		processOpenDialogKeypress(c);
		return;
	}

	if (state.goto_dialog.active) {
		processGotoDialogKeypress(c);
		return;
	}

	if (state.pal.open) {
		processPaletteKeypress(c);
		return;
	}

	switch (c) {
		case '\r':
            be_insertNewLine();
			break;

		case CTRL_KEY('q'):
			be_quitEditor();
			break;

		case CTRL_KEY('s'):
			be_saveFile(NULL);
			break;

        case CTRL_SHIFT_S:
			be_saveDialog_open(&state.save_dialog);
            return;

		case CTRL_KEY('o'):
			be_openDialog_open(&state.open_dialog);
			return;

		case CTRL_KEY('g'):
			be_gotoDialog_open(&state.goto_dialog);
			return;

		case CTRL_KEY('p'):
			be_paletteOpen();
			break;

		// HOME/END move within the current line; CTRL_HOME/CTRL_END jump
		// to the start/end of the whole document.
        case ALT_A:
		case HOME:
			state.cur_x = 0;
			break;

        case ALT_E:
		case END:
			if (state.cur_y < state.numrows) state.cur_x = state.row[state.cur_y].size;
			break;

        case CTRL_KEY('a'):
	    case CTRL_HOME:
		    state.cur_x = 0;
			state.cur_y = state.def_y;
		    break;

        case CTRL_KEY('e'):
		case CTRL_END:
			state.cur_y = state.numrows ? state.numrows - 1 : 0;
			state.cur_x = state.numrows ? state.row[state.cur_y].size : 0;
		    break;

		case BACKSPACE:
		case DELETE:
            if (c == DELETE) be_handleArrowKeys(ARROW_RIGHT);
            be_deleteChar();
            break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				int lastrow = state.numrows ? state.numrows - 1 : 0;
			    if (c == PAGE_UP) {
					state.cur_y = state.rowoff;
				} else if (c == PAGE_DOWN) {
				    state.cur_y = state.rowoff + state.screenrows - 1;
    				if (state.cur_y > state.numrows) state.cur_y = lastrow;
			    }

				int times = state.screenrows / 2;
				while (times--) {
					be_handleArrowKeys(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
				}
			}
			break;
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			be_handleArrowKeys(c);
			break;

		case WORD_RIGHT:
			be_word_move_forward();
			break;

		case WORD_LEFT:
			be_word_move_backward();
			break;

		case CTRL_KEY('w'):
		case CTRL_BACKSPACE:
			be_delete_word_backward();
			break;

		case WORD_DELETE:
			be_delete_word_forward();
			break;

		case CTRL_KEY('l'):
		case '\x1b':
			break;

		default:
			be_editorInsertChar(c);
			break;

	}
}

/* Row Operations */
void be_updateRow(BE_Row *row) {
    int j;
    int tabs = 0;
    for (j = 0; j < row->size; j++)
        if (row->data[j] == '\t') tabs++;

    if (row->render != NULL) free(row->render);
    row->render = malloc(row->size + tabs*(DEFAULT_TAB_SIZE - 1) + 1);
    if (!row->render) be_die("malloc");

    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->data[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % DEFAULT_TAB_SIZE != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->data[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void be_insertRow(int at, char *s, size_t len) {
    if (at < 0 || at > state.numrows) return;

	BE_Row *new_row = realloc(state.row, sizeof(BE_Row) * (state.numrows + 1));
	if (!new_row) { free(state.row); be_die("realloc"); }

	state.row = new_row;
	memmove(&state.row[at+1], &state.row[at], sizeof(BE_Row) * (state.numrows - at));

	state.row[at].size = len;
	state.row[at].data = malloc(len+1);
	if (!state.row[at].data) be_die("malloc");
	memcpy(state.row[at].data, s, len);
	state.row[at].data[len] = '\0';

	state.row[at].rsize = 0;
	state.row[at].render = NULL;
	be_updateRow(&state.row[at]);

	state.numrows++;
    state.dirty++;
}

void be_insertNewLine() {
    if (state.cur_x == 0) {
        be_insertRow(state.cur_y, "", 0);
    } else {
        BE_Row *row = &state.row[state.cur_y];
        be_insertRow(state.cur_y + 1, &row->data[state.cur_x], row->size - state.cur_x);
        row = &state.row[state.cur_y];
        row->size = state.cur_x;
        row->data[row->size] = '\0';
        be_updateRow(row);
    }
    state.cur_y++;
    state.cur_x = 0;
}
    

char *be_rowsToString(size_t *buflen) {
	int totlen = 0;
	int i;
	for (i = 0; i < state.numrows; i++) {
		totlen += state.row[i].size + 1;
	}
	*buflen = totlen;

	char *buf = malloc(totlen);
	if (!buf) be_die("malloc");
	char *p = buf;

	for (i = 0; i < state.numrows; i++) {
		memcpy(p, state.row[i].data, state.row[i].size);
		p += state.row[i].size;
		*p = '\n';
		p++;
	}
	return buf;
}

void be_freeRow(BE_Row *row) {
    free(row->render);
    free(row->data);
}

// Frees every row in the buffer and resets numrows to 0, so a subsequent
// open can start from a clean slate instead of appending onto whatever
// was already loaded.
void be_clearRows(void) {
	for (int i = 0; i < state.numrows; i++) be_freeRow(&state.row[i]);
	state.numrows = 0;
}

void be_deleteRow(int at) {
    if (at < 0 || at >= state.numrows) return;
    be_freeRow(&state.row[at]);

    memmove(&state.row[at], &state.row[at+1], sizeof(BE_Row) * (state.numrows - at - 1));
    state.numrows--;
    state.dirty++;
}

void be_rowAppendString(BE_Row *row, char *s, size_t len) {
    char *new_data = realloc(row->data, row->size + len + 1);
    if (!new_data) { free(row->data); be_die("realloc"); }
    row->data = new_data;
    memcpy(&row->data[row->size], s, len);
    row->size += len;
    row->data[row->size] = '\0';
    be_updateRow(row);
    state.dirty++;
}

void be_rowInsertChar(BE_Row *row, int at, int c) {
	if (at < 0 || at > row->size) at = row->size;
	char *new_data = realloc(row->data, row->size + 2);
	if (!new_data) { free(row->data); be_die("realloc"); }
	row->data = new_data;
	memmove(&row->data[at+1], &row->data[at], row->size - at + 1);
	row->size++;
	row->data[at] = c;
	be_updateRow(row);
    state.dirty++;
}

void be_editorInsertChar(int c) {
	if (state.cur_y == state.numrows) {
		be_insertRow(state.numrows, "", 0);
	}
	// be_moveCursor(sb, int x, int y)
	be_rowInsertChar(&state.row[state.cur_y], state.cur_x, c);
	state.cur_x++;
}

void be_rowDeleteChar(BE_Row *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->data[at], &row->data[at+1], row->size - at);
    row->size--;
    be_updateRow(row);
    state.dirty++;
}

void be_deleteChar() {
    if (state.cur_y == state.numrows) return;
    if (state.cur_x == 0 && state.cur_y == 0) return;
    
    BE_Row *row = &state.row[state.cur_y];
    
    if (state.cur_x > 0) {
        be_rowDeleteChar(row, state.cur_x - 1);
        state.cur_x--;
    } else {
        // Merge with previous line
        state.cur_x = state.row[state.cur_y - 1].size;
        be_rowAppendString(&state.row[state.cur_y - 1], row->data, row->size);
        be_deleteRow(state.cur_y);
        state.cur_y--;
    }
}



/* File I/O */
bool validFilename(const char *filename) {
	if (filename == NULL || filename[0] == '\0') return false;
	if (strlen(filename) > PATH_MAX) return false;

	const char *p = filename;
	while (*p) {
		const char *slash = strchr(p, '/');
		size_t len = slash ? (size_t)(slash - p) : strlen(p);
		if (len > NAME_MAX) return false;
		if (!slash) break;
		p = slash + 1;
	}
	return true;
}

bool checkFileExists(const char *filepath) {
	struct stat st;
	return stat(filepath, &st) == 0;
}

int isDir(const char *filename) {
	if (!validFilename(filename)) return -1; // Invalid filename error
	if (!checkFileExists(filename)) return 0;
	struct stat st;
	if (stat(filename, &st) != 0) return -2; // Stat Error
	return S_ISDIR(st.st_mode);
}


void be_openFile(const char *filename) {
	if (state.filename != NULL) free(state.filename);
	state.filename = strdup(filename);

	FILE *fp = fopen(filename, "r");
	if (!fp) be_die("Could not open file !!");
	state.mode = EDIT;

	be_clearRows(); // start from an empty buffer, don't append onto rows from a previously open file

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
			linelen--;
		be_insertRow(state.numrows, line, linelen);
	}

	free(line);
	fclose(fp);
    state.dirty = 0;
}

void be_openBlankFile() {
	be_clearRows(); // start from an empty buffer, don't append onto rows from a previously open file
	be_insertRow(state.numrows, "", 0);
	if (state.filename != NULL) free(state.filename);
	state.filename = NULL;
	state.cur_x = 0;
	state.cur_y = state.def_y;
	state.dirty = 0;
	state.mode = EDIT;
}


bool be_saveFile(size_t *out_len) {
	if (state.filename == NULL && state.mode != SPLASH) {
		state.save_dialog.active = true;
		return false;
	}

	size_t len;
	char *buf = be_rowsToString(&len);
	bool ok = false;
	int saved_errno = 0;

	int fd = open(state.filename, O_RDWR | O_CREAT, 0644);
	if (fd != -1) {
		if (ftruncate(fd, len) != -1 && write(fd, buf, len) == (ssize_t)len) {
			ok = true;
		} else {
			saved_errno = errno;
		}
		close(fd); // always close, whether or not the write succeeded
	} else {
		saved_errno = errno;
	}

	if (ok) {
		state.dirty = 0;
		// Set Status message :: Success
		snprintf(state.statusmsg, sizeof(state.statusmsg), "✓  Saved %ld bytes to file: %.20s", len, state.filename);
		if (out_len) *out_len = len;
	} else {
		// Set Status message :: Failed
		snprintf(state.statusmsg, sizeof(state.statusmsg), "[X] I/O Error Could not save file to disk : %s", strerror(saved_errno));
	}
	state.statusmsg_time = time(NULL);
	free(buf);
	return ok;
}


/* Editor Ops */
int be_calculateRx(BE_Row *row, int cx) {
    int rx = 0;
    for (int j = 0; j < cx; j++) {
        if (row->data[j] == '\t')
            rx += (DEFAULT_TAB_SIZE - 1) - (rx % DEFAULT_TAB_SIZE);
        rx++;
    }
    return rx;
}

void editorScroll() {
    if (state.cur_y < state.numrows) {
      state.row_x = be_calculateRx(&state.row[state.cur_y], state.cur_x);
    }

    if (state.cur_y < state.rowoff) {
		state.rowoff = state.cur_y;
	}
	if (state.cur_y >= state.rowoff + state.screenrows) {
		state.rowoff = state.cur_y - state.screenrows + 1;
	}
	if (state.row_x < state.coloff) {
		state.coloff = state.row_x;
	}
	if (state.row_x >= state.coloff + state.screencols - state.def_x) {
		state.coloff = state.row_x - state.screencols + 1;
	}
}

void be_setStatusMsg(const char *msg) {
	snprintf(state.statusmsg, sizeof(state.statusmsg), "%s", msg);
	state.statusmsg_time = time(NULL);
}

void be_refreshScreen() {
	editorScroll();

	// Append a string builder on the stack so that it can be used to store all write-ops !!
	StringBuilder sb;
	sb_init(&sb);

	// Clear Screen Calls :: Before drawing rows
	sb_append(&sb, "\x1b[?25l");
	sb_append(&sb, "\x1b[H");

	// Draw Rows !!
	be_drawRows(&sb);
	be_drawStatusBar(&sb);

	if (state.mode == SPLASH) {
		be_drawHomepage(&sb);
		be_moveCursor(&sb, 0, state.screenrows);
	} else if (!state.save_dialog.active && !state.open_dialog.active) {
		char buf[32];
		snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
				(state.cur_y - state.rowoff) + 1,
				(state.row_x - state.coloff) + 1 + state.def_x);
		sb_append(&sb, buf);
		sb_append(&sb, "\x1b[?25h");
	}

    if (state.save_dialog.active) {
        int dlg_x, dlg_y;
        be_drawSaveDialog(&sb, &dlg_x, &dlg_y);

		if (state.save_dialog.focus == SD_FOCUS_INPUT) {
			char buf[32];
			snprintf(buf, sizeof(buf), "\x1b[%d;%dH", dlg_y + 1, dlg_x + 1);
			sb_append(&sb, buf);
			sb_append(&sb, "\x1b[?25h");
		}
    }

	if (state.open_dialog.active) {
		int dlg_x, dlg_y;
		be_drawOpenDialog(&sb, &dlg_x, &dlg_y);

		if (state.open_dialog.focus == OD_FOCUS_INPUT) {
			char buf[32];
			snprintf(buf, sizeof(buf), "\x1b[%d;%dH", dlg_y + 1, dlg_x + 1);
			sb_append(&sb, buf);
			sb_append(&sb, "\x1b[?25h");
		}
	}

	if (state.goto_dialog.active) {
		int dlg_x, dlg_y;
		be_drawGotoDialog(&sb, &dlg_x, &dlg_y);

		if (state.goto_dialog.focus == GD_FOCUS_INPUT) {
			char buf[32];
			snprintf(buf, sizeof(buf), "\x1b[%d;%dH", dlg_y + 1, dlg_x + 1);
			sb_append(&sb, buf);
			sb_append(&sb, "\x1b[?25h");
		}
	}

	if (state.pal.open) {
		state.save_dialog.active = false;
        int cmd_x, cmd_y;
		be_drawPalette(&sb, &cmd_x, &cmd_y);
		char buf[32];
		snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cmd_y + 1, cmd_x + 1);
		sb_append(&sb, buf);
		sb_append(&sb, "\x1b[?25h");
	}

	size_t res = write(STDOUT_FILENO, sb.data, sb.len);
	be_check_and_raise(res == sb.len, "Could not clear screen", BE_ERR_RENDER);

	sb_free(&sb);
}

/* Initialization && Closing */
void be_freeEditor(void) {
	for (int i = 0; i < state.numrows; i++) {
		free(state.row[i].data);
		free(state.row[i].render);
	}
	free(state.row);
	state.row = NULL;
	state.numrows = 0;

	free(state.filename);
	state.filename = NULL;

	state.dirty = 0;
}

void be_initEditor(void) {
	state.def_x = GUTTER_WIDTH;
	state.def_y = 0;
	state.cur_x = 0;
	state.cur_y = 0;
	state.row_x = 0;

	state.rowoff = 0;
	state.coloff = 0;
	state.dirty = 0;

	state.numrows = 0;
	state.row = NULL;
	state.filename = NULL;
	state.mode = SPLASH;

	// Cmd Palette Init
	state.pal.open = false;
	state.pal.max_rows = 7;
	state.pal.nfiltered = 7;

	atexit(be_freeEditor);

	if (be_getWindowSize(&state.screenrows, &state.screencols) == -1) be_die("be_getWindowSize");
	state.screenrows -= 1;
}

void be_quitNow(const char *message) {
	// Get the dialog out of the way so the message is what's visible.
	state.save_dialog.active = false;

	snprintf(state.statusmsg, sizeof(state.statusmsg), "%s", message ? message : "Quitting ...");
	state.statusmsg_time = time(NULL);
	be_refreshScreen();

	struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 200000000L; // 200,000,000 ns = 0.2 s
    nanosleep(&ts, NULL);

	int res = write(STDOUT_FILENO, "\x1b[2J", 4);
	res += write(STDOUT_FILENO, "\x1b[H", 3);
	be_check_and_raise(res == 7, "Could not the clear screen", BE_ERR_RENDER);

	be_freeEditor();
	exit(0);
}

void be_quitEditor(void) {
	if (state.dirty != 0) {
		be_saveDialog_open(&state.save_dialog);
		state.save_dialog.quit_on_save = true;
		return;
	}

	be_quitNow(NULL);
}

/* Main Loop */
int main(int argc, char **argv) {
    be_enableRawMode();
	be_initEditor();

	// Open the file
	if (argc >= 2) {
		char *filepath = argv[1];
		if (!validFilename(filepath)) be_setStatusMsg("{X} Invalid filename provided");
		int is_dir_res = isDir(filepath);
		if (is_dir_res == -1) be_setStatusMsg("{X} Invalid filename provided");
		else if (is_dir_res == -2) be_setStatusMsg("{X} Error occured while opening");
		else if (is_dir_res == 1) be_setStatusMsg("{X} Provided path is a directory");
		else {
			if (checkFileExists(filepath)) be_openFile(argv[1]);
			else {
				be_openBlankFile();
				state.filename = strdup(filepath);
			}
		}
	}
    while (1) {
		be_refreshScreen();
		be_processKeypress();
    }
    return 0;
}
