/* Includes */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdbool.h>

/* Defines */

#define _BSD_SOURCE
#define _GNU_SOURCE

#define CTRL_KEY(k) ((k) & 0x1f)
#define BASIC_EDITOR_VERSION "1.0.0"

#define B_H   "─"   /* horizontal              */
#define B_V   "│"   /* vertical                */
#define B_TL  "╭"   /* top-left  (rounded)     */
#define B_TR  "╮"   /* top-right (rounded)     */
#define B_BL  "╰"   /* bot-left  (rounded)     */
#define B_BR  "╯"   /* bot-right (rounded)     */

#define MAX_SPANS 6

// Highlights for Homepage
#define S_DIM    "38;5;240"
#define S_TEXT   "38;5;250"
#define S_KEY    "1;38;5;252"
#define S_ACCENT "38;2;133;153;0"

/* Data */

typedef enum {
	BE_ERR_ASSERT,
	BE_ERR_RENDER,
	BE_ERR_TERM,
	BE_ERR_OP
} BE_ErrorKind;


typedef enum {
	ARROW_LEFT  = 1000,
	ARROW_RIGHT,
	ARROW_DOWN,
	ARROW_UP,
	PAGE_UP,
	PAGE_DOWN,
	HOME,
	END,
	DELETE,
	BACKSPACE,
	RETURN
} BE_Key;


typedef enum {
	AL_CENTER,
	AL_BLOCK
} BE_Align;


typedef struct {
	const char *style;
	const char *text;
} BE_Span;


typedef struct {
	BE_Span  spans[MAX_SPANS];
	int 	 nspans;
	BE_Align align;
} BE_Line;


/* Homepage Lines */
static const BE_Line homepage[] = {
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


typedef struct {
	// int cx, cy;
	int size;
	char *data;
} BE_Row;

typedef struct {
	int    	def_x, def_y;
	int    	cur_x, cur_y;
	int    	rowoff;
	int 	coloff;
	int    	screenrows;
	int    	screencols;
	int    	numrows;
	BE_Row 	*row;
	struct 	termios orig_termios;
} BE_State;

static BE_State state;

/* String Builder */
typedef struct {
	char *data;
	size_t len;
	size_t cap;
} StringBuilder;

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
                sb->data = realloc(sb->data, new_cap);
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
	int res = write(STDOUT_FILENO, "\x1b[2J", 4);
	res += write(STDOUT_FILENO, "\x1b[H", 3);
	be_check_and_raise(res != 7, "Could not clear screen for be_die function", BE_ERR_OP);

    perror(s);
    exit(1);
}

void be_disableRawMode(void) {
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
}

int be_readKey() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) be_die("read");
	}
	
	if (c == '\x1b') {
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			if (seq[1] > '0' && seq[1] < '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME;
						case '3': return DELETE;
						case '4': return END;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME;
						case '8': return END;
					}
				}

			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME;
					case 'F': return END;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H': return HOME;
				case 'F': return END;
			}
		}
		return '\x1b';
	} else {
		return c;
	}
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
        if (y >= state.screenrows) y = state.screenrows - 1;

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

static int line_width(const BE_Line *ln) {
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
			else {
				sb_append(sb, "Made with ♡ by srivsatava.s");
			}
		}
		else {
			// Determine line length
			int len = state.row[filerow].size - state.coloff;
			if (len < 0) len = 0;
			if (len > state.screencols) len = state.screencols;
			// Append line-no to the line
			char linenum[32];
			snprintf(linenum, sizeof(linenum), "%s%5d│ %s", "\x1b["S_DIM"m", y+1, "\x1b[0m");
			sb_append(sb, linenum);

			sb_append(sb, &state.row[filerow].data[state.coloff]);
			res = 0;
		}
		sb_append(sb, "\x1b[K");
		if (y < state.screenrows - 1) sb_append(sb, "\r\n");
	}

	// Bring back cursor :: After drawing rows
	sb_append(sb, "\x1b[H");
	sb_append(sb, "\x1b[?25h");
	return res;
}

void be_drawWelcomeMsg(StringBuilder *sb, const char *msg) {
	/* 
	* Doing a redundant check for now just to make sure logo does not render
	* on file open ...
	*/
	if (state.numrows == 0) {
		int wlcmlen = strlen(msg);
		int cx = (state.screencols - wlcmlen)/2;
		int cy = state.screenrows / 2;
		be_moveCursor(sb, cx, cy);
		sb_append(sb, msg);

		// Bring back cursor :: After drawing message
		sb_append(sb, "\x1b[H");
		sb_append(sb, "\x1b[?25h");
	}
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
		const BE_Line *ln = &homepage[i];
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

	// Move cursor back to start
	be_moveCursor(sb, state.def_x, state.def_y);
}

void editorScroll() {
	if (state.cur_y < state.rowoff) {
		state.rowoff = state.cur_y;
	}
	if (state.cur_y >= state.rowoff + state.screenrows) {
		state.rowoff = state.cur_y - state.screenrows + 1;
	}
	if (state.cur_x < state.coloff) {
		state.coloff = state.cur_x;
	}
	if (state.cur_x >= state.coloff + state.screencols) {
		state.coloff = state.cur_x - state.screencols + 1;
	}
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
	int draw_wlcm = be_drawRows(&sb);

	if (draw_wlcm) {
		// Move Cursor to the center and write something
		// const char *msg = "|> HELLO FROM BE !!\r\n";
		// be_drawWelcomeMsg(&sb, msg);
		be_drawHomepage(&sb);
	}

	// Handle cursor movement
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
			(state.cur_y - state.rowoff) + 1, (state.cur_x - state.coloff) + 1);
	sb_append(&sb, buf);

	size_t res = write(STDOUT_FILENO, sb.data, sb.len);
	be_check_and_raise(res == sb.len, "Could not clear screen", BE_ERR_RENDER);

	sb_free(&sb);
}


/* Input Processing */
void be_handleArrowKeys(int key) {
	BE_Row *row = (state.cur_y >= state.numrows) ? NULL : &state.row[state.cur_y];

	switch (key) {
		case ARROW_LEFT:
			if (state.cur_x != state.def_x) {
				state.cur_x--;
			} else if (state.cur_y > 0){
				state.cur_y--;
				state.cur_x = state.row[state.cur_y].size + state.def_x;
			}
			break;
		case ARROW_RIGHT:
			if (row && state.cur_x < row->size + state.def_x) {
				state.cur_x++;
			} else if (row && state.cur_x == row->size) {
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
	if (state.cur_x > rowlen + state.def_x) {
		state.cur_x = rowlen + state.def_x;
	}
}


void be_processKeypress() {
	int res;
	int c = be_readKey();
	BE_Row *row;
	switch (c) {
		case CTRL_KEY('q'):
			res = write(STDOUT_FILENO, "\x1b[2J", 4);
			res += write(STDOUT_FILENO, "\x1b[H", 3);
			be_check_and_raise(res == 7, "Could not the clear screen", BE_ERR_RENDER);
			exit(0);
			break;

		case HOME:
			state.cur_x = state.def_x;
			break;
		case END:
			row = (state.cur_y >= state.numrows) ? NULL : &state.row[state.cur_y];
			int rowlen = row ? row->size : 0;
			state.cur_x = rowlen + state.def_x;
			// state.cur_x = state.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
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
	}
}

/* Row Operations */
void be_appendRow(char *s, size_t len) {
	state.row = realloc(state.row, sizeof(BE_Row) * (state.numrows + 1));
	int at = state.numrows;
	state.row[at].size = len;
	state.row[at].data = malloc(len+1);
	memcpy(state.row[at].data, s, len);
	state.row[at].data[len] = '\0';
	state.numrows++;
}

/* File I/O */
void be_openFile(const char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) be_die("Could not open file !!");

	char *line = NULL; 
	size_t linecap = 0;
	ssize_t linelen;

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
			linelen--;
		be_appendRow(line, linelen);
	}

	free(line);
	fclose(fp);
}

/* Initialization */
void be_initEditor() {
	state.def_x = 0;
	state.def_y = 0;
	state.cur_x = 0;
	state.cur_y = 0;

	state.rowoff = 0;
	state.coloff = 0;

	state.numrows = 0;
	state.row = NULL;


	if (be_getWindowSize(&state.screenrows, &state.screencols) == -1) be_die("be_getWindowSize");
}

/* Main Loop */

int main(int argc, char **argv) {
    be_enableRawMode();
	be_initEditor();

	if (argc >= 2) {
		be_openFile(argv[1]);
	}
	if (state.numrows > 0) {
		state.def_x = 7;
		state.cur_x = state.def_x;
	}

    while (1) {
		be_refreshScreen();
		be_processKeypress();
    }
    return 0;
}
