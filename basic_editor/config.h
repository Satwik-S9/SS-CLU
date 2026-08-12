// #define LIGHT_THEME_MODE
#define DARK_THEME_MODE

struct StatusBar_Theme {
	char *sb_background;
	char *sb_foreground;
};

#ifdef DARK_THEME_MODE
	#define STATUS_BAR_BACKGROUND "#2d2d2d"
	#define STATUS_BAR_FOREGROUND "#818181"
#endif

#ifdef LIGHT_THEME_MODE
	#define STATUS_BAR_BACKGROUND "#e1e1e1"
	#define STATUS_BAR_FOREGROUND "#2d2d2d"
#endif
