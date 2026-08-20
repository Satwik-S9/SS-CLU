#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#define ESC '\x1b'

static struct termios orig;

static void restoreTerminal(void) {
    /* Undo enableMouse()'s two writes and go back to cooked mode, or the
     * user's shell inherits raw mode + mouse reporting after we exit. */
    write(STDOUT_FILENO, "\x1b[?1006l\x1b[?1000l", 16);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

static void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig);
    atexit(restoreTerminal);

    struct termios raw = orig;
    raw.c_lflag &= ~(ECHO | ICANON);   /* no local echo, read byte-by-byte */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

typedef enum {
	LEFT_CLICK = 1000,
	RIGHT_CLICK,
	SCROLL,
} MouseClickType;

typedef struct {
	int startx, starty;
	int endx, endy;
	int mode;
} MouseClick;

MouseClick g_mouseclick = { 0 };

int readMouseEvents() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) {
			perror("readKey");
			exit(1);
		}
	}

	if (c != ESC) return (unsigned char)c;
	char idf[2]; // Identifier
	if (read(STDIN_FILENO, &idf, 2) != 2) return ESC;

	if (idf[0] == '[' && idf[1] == '<') {
		short n = 0;
		char seq[16];
		char ch, final = 0;
		while (n < sizeof(seq) - 1) {
			if (read(STDIN_FILENO, &ch, 1) != 1) return ESC;
			if (ch >= 0x40 && ch <= 0x7E) {
				final = ch;
				break;
			}
			seq[n++] = ch;
		}
		seq[n] = '\0';
		if (final == 0) return ESC;
		int mode1 = -1, mode2 = -1;
		if (final == 'm') {
			sscanf(seq, "%d;%d;%d", &mode1, &g_mouseclick.startx, &g_mouseclick.starty);
		}
		if (final == 'm') {
			sscanf(seq, "%d;%d;%d", &mode2, &g_mouseclick.endx, &g_mouseclick.endy);
		}
		if (mode1 != mode2) g_mouseclick.mode = mode2;
		else g_mouseclick.mode = mode1;
		
		char buf[128];
		switch (final) {
			case 'M':
				snprintf(buf, sizeof(buf), 
					"Read Seq: %s, Parsed :: Click Start -- m: %d, cx: %d, cy: %d\r\n", 
					seq, g_mouseclick.mode, g_mouseclick.startx, g_mouseclick.starty);
				write(STDOUT_FILENO, buf, strlen(buf));
				return LEFT_CLICK;

			case 'm':
				snprintf(buf, sizeof(buf), 
					"Read Seq: %s, Parsed :: Click End -- m: %d, cx: %d, cy: %d\r\n", 
					seq, g_mouseclick.mode, g_mouseclick.startx, g_mouseclick.starty);
				write(STDOUT_FILENO, buf, strlen(buf));
				return LEFT_CLICK;
		}
	}
	return ESC;
}

int main() {
	enableRawMode();
	char buf[32];

    write(STDOUT_FILENO, "\x1b[?1000h\x1b[?1006h", 16);
	while (1) {
		int key = readMouseEvents();
		switch(key) {
			case 'q':
				return 0;

			case LEFT_CLICK:
				snprintf(buf, sizeof(buf), "\x1b[%d;%dH", g_mouseclick.endy + 1, g_mouseclick.endx + 1);
				write(STDOUT_FILENO, buf, sizeof(buf));
				break;
		}
		buf[0] = '\0';
	}
	return 0;
}
