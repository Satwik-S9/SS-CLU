/* Theme Settings */
// #define LIGHT_THEME_MODE  /* Enable Light Mode */
#define DARK_THEME_MODE   /* Enable Dark Mode */

/* Colors for for dark theme mode */
#ifdef DARK_THEME_MODE

	/** Status Bar Colors **/
	#define STATUS_BAR_BACKGROUND "#2d2d2d"
	#define STATUS_BAR_FOREGROUND "#818181"

	/** Dialog box Colors **/
	#define DIALOG_BOX_OK		"#859900"
	#define DIALOG_BOX_ERR		"#db302e"
	#define DIALOG_BOX_DIM		"#585858"
	#define DIALOG_BOX_SEL		"#323232"
	#define DIALOG_BOX_TEXT		"#bcbcbc"
	#define DIALOG_BOX_FILE		"#969696" // highlight color for files shown in the open dialog box
	#define DIALOG_BOX_ACCENT	"#859900" // Color for buttons and carets

	/** Home Screen Colors **/
	#define HOMESCREEN_KEY		"#d0d0d0"
	#define HOMESCREEN_DIM		"#585858"
	#define HOMESCREEN_TEXT		"#bcbcbc"
	
#endif

/* Colors for the light theme mode */
#ifdef LIGHT_THEME_MODE

	/** Status Bar Colors **/
	#define STATUS_BAR_BACKGROUND "#e1e1e1"
	#define STATUS_BAR_FOREGROUND "#2d2d2d"

	/** Dialog box Colors **/
	#define DIALOG_BOX_OK		"#859900"
	#define DIALOG_BOX_ERR		"#db302e" // Color for error dialogs and options
	#define DIALOG_BOX_DIM		"#585858"
	#define DIALOG_BOX_SEL		"#bcbcbc" // Color for selected options
	#define DIALOG_BOX_TEXT		"#323232"
	#define DIALOG_BOX_FILE		"#272727" // highlight color for files shown in the open dialog box
	#define DIALOG_BOX_ACCENT	"#859900" // Color for buttons and carets

	/** Home Screen Colors **/
	#define HOMESCREEN_KEY		"#969696"
	#define HOMESCREEN_DIM		"#585858"
	#define HOMESCREEN_TEXT		"#a5a5a5"

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

/* Mouse Settings for the editor */
#define ENABLE_MOUSE_SUPPORT 1 /* Enable (1) or Disable (0) mouse support for the editor */
#define MOUSE_SCROLL_DELTA   4 /* How many columns should the mouse scroll */

/* Default tab size for the editor */
#define DEFAULT_TAB_SIZE  4
