#ifndef _BE_H_
#define _BE_H_

/* Includes */
#include <stddef.h>
#include <time.h>
#include <termio.h>

/* Macro Definitions */
#define BASIC_EDITOR_VERSION "1.0.2"

#define MAX_SPANS 		6
#define GUTTER_WIDTH 	7
#define MAX_INPUT_SIZE 	256

/** Borders for floats **/
#define B_H   "─"
#define B_V   "│"
#define B_TL  "╭"
#define B_TR  "╮"
#define B_BL  "╰"
#define B_BR  "╯"
#define B_LT  "├"
#define B_RT  "┤"


/* Colorscheme Model */
/* TODO: Should migrate this to a central colorscheme struct ... */
struct StatusBar_Theme {
	char *sb_background;
	char *sb_foreground;
};

typedef struct { int r, g, b; } Color;

typedef struct {
	// Status Bar Colors
	struct { Color fg; Color bg; } status;

	// Dialog Box Colors
	struct { 
		Color ok;
		Color err; 
		Color dim; 
		Color sel;
		Color text;
		Color file;
		Color accent;
	} dialog;

	// Splash Screen Colors
	struct { 
		Color dim;
		Color text;
		Color key;
	} homepage;
} Colorscheme;

/* Data Models */
/** Generic Models **/
/*** String Builder ***/
typedef struct {
	char *data;
	size_t len;
	size_t cap;
} StringBuilder;

/** BE Specific Data Models **/
/*** Enumerations ***/
typedef enum {
	BE_ERR_ASSERT,
	BE_ERR_RENDER,
	BE_ERR_TERM,
	BE_ERR_FILE_OP,
	BE_ERR_GENERIC_OP
} BE_ErrorKind;


typedef enum {
	// Keys present in ASCII
	BACKSPACE = 0x7f,

    // Nav Keys
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
	CTRL_BACKSPACE,

	// Special Mod Keys
	CTRL_SHIFT_S,
    ALT_A,
    ALT_E,

	// Mouse Clicks
	M_LEFT_CLICK,
	M_OTHER_CLICK,
	M_SCROLL_UP,
	M_SCROLL_DOWN
} BE_Key;

/* Modes:
* 0: Left Click,
* 2: Right Click, 
* 64: Scroll Up, 
* 65: Scroll Dowm 
* */
typedef struct { 
	int 	sx, sy;
	int 	ex, ey;
	int 	mode;
} BE_MouseEvent;

typedef enum {
	SPLASH,
	EDIT
} BE_Mode;

/** Splash Screen Structures **/
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
} BE_HomepageLine;

/** BE Dialog Boxes **/
/*** Save Dialog Box ***/
typedef enum {
	SD_FOCUS_INPUT,
	SD_FOCUS_SAVE,
	SD_FOCUS_QUIT,
	SD_FOCUS_CANCEL,
	SD_FOCUS_COUNT
} BE_SaveDialogFocus;

typedef struct {
	int					cursor;
    bool   				active;
	bool   				quit_on_save;
	BE_SaveDialogFocus 	focus;
    char   				input_path[MAX_INPUT_SIZE + 1];
	int 				inputlen;
} BE_SaveDialog;


/*** Open Dialog Box ***/
typedef enum {
	OD_FOCUS_INPUT,
	OD_FOCUS_OPEN,
	OD_FOCUS_CANCEL,
	OD_FOCUS_COUNT,
} BE_OpenDialogFocus;

typedef struct {
	int 				cursor;
	bool 				active;
	bool 				create_file;
	BE_OpenDialogFocus 	focus;
	char 				input_path[MAX_INPUT_SIZE + 1];
	int					inputlen;
} BE_OpenDialog;

/*** Goto Dialog Box ***/
typedef enum {
	GD_FOCUS_INPUT,
	GD_FOCUS_GOTO,
	GD_FOCUS_CANCEL,
	GD_FOCUS_COUNT,
} BE_GotoDialogFocus;

typedef struct {
	BE_GotoDialogFocus focus;
	int  cursor;
	bool active;
	int  inputlen;
	char input[32];
} BE_GotoDialog;

/*** Find Dialog Box ***/
typedef struct { int row, col; } BE_Match;   

typedef struct {
	bool 		active;
	int			cursor;
	int  		inputlen;
	int 		nmatches;
	int			cur;
	int 		saved_cur_x, saved_cur_y, saved_rowoff, saved_coloff;
	BE_Match 	*matches;
	char 		input[MAX_INPUT_SIZE + 1];
} BE_Find;

/*** Command Palette ***/
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
	{ CMD_SAVEAS, 	"save file as",		"^⇧ S" },
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
	int  cursor;
	int  qlen;
	int  max_rows;
	int  sel;
	int  nfiltered;
	int  filtered[NCOMMANDS];
} BE_CmdPalette;

/** BE Main Structures **/
/*** Editor Row ***/
typedef struct {
	// int cx, cy;
	int size;
	int rsize;
	char *data;
	char *render;
} BE_Row;

/*** the editor state ***/
typedef struct {
	int    			def_x, def_y;
	int    			cur_x, cur_y;
	int				loc_x, loc_y;
	int     		row_x;
	int    			rowoff;
	int 			coloff;
	int    			screenrows;
	int    			screencols;
	int    			numrows;
	int	    		dirty;
	BE_Mode			mode;
	BE_MouseEvent	mouse;
	BE_GotoDialog   goto_dialog;
	BE_OpenDialog	open_dialog;
    BE_SaveDialog 	save_dialog;
	BE_Find 		find;	
	BE_CmdPalette   pal;
	BE_Row 			*row;
	char			*filename;
	char			statusmsg[80];
	time_t			statusmsg_time;
	struct termios 	orig_termios;
} BE_State;

/* Forward Declarations for Important Functions */
/** General **/
void be_die(const char *s);

/** Editor Open/Close **/
void be_quitEditor(void);
void be_quitNow(const char *message);

/** Editor I/O **/
void be_deleteChar();
void be_editorInsertChar(int c);
void be_insertNewLine();

bool validFilename(const char *filename);
bool checkFileExists(const char *filepath);
int isDir(const char *filename);

/** Editor File Ops **/
void be_openBlankFile();
void be_openFile(const char *filename);
bool be_saveFile(size_t *out_len);

/** Editor Render **/
int be_calculateRx(BE_Row *row, int cx);

void be_drawSaveDialog(StringBuilder *sb, int *cur_x, int *cur_y);
void be_drawRowWithMatches(StringBuilder *sb, int filerow, int coloff, int len);
void be_setStatusMsg(const char *msg);
void be_clearRows(void);
void be_updateRow(BE_Row *row);

#endif // _BE_H_
