// TODO: Convert all colors to hexcodes and add a colorscheme struct to be.h so that we can build colorscheme on launch of editor
/* Theme Settings */
// #define LIGHT_THEME_MODE  /* Enable Light Mode */
#define DARK_THEME_MODE   /* Enable Dark Mode */

/* Status Bar Colors for dark theme mode */
#ifdef DARK_THEME_MODE
	#define STATUS_BAR_BACKGROUND "#2d2d2d"
	#define STATUS_BAR_FOREGROUND "#818181"
#endif

/* Status Bar Colors for light theme mode */
#ifdef LIGHT_THEME_MODE
	#define STATUS_BAR_BACKGROUND "#e1e1e1"
	#define STATUS_BAR_FOREGROUND "#2d2d2d"
#endif


/* Highlights for Homepage -- Splash Screen */
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
#define S_ERROR  "38;2;220;50;47"
#define S_FILE 	 "38;2;114;114;114"
#define S_RESET  "\x1b[0m"

/* Highlights for dialog boxes and command palette */
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
#define PAL_ERROR  "\x1b[38;2;220;50;47m"


/* Mouse Settings for the editor */
#define ENABLE_MOUSE_SUPPORT 1 /* Enable (1) or Disable (0) mouse support for the editor */
#define MOUSE_SCROLL_DELTA   4 /* How many columns should the mouse scroll */
