#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <locale.h>

int MODE_PLAYLISTS          = 0;
int MODE_LIST               = 1;
int MODE_UPCOMING           = 2;

int MODE_MAX                = 3;

typedef struct Song Song;
typedef struct Key Key;

struct Key {
	int key;
	void (*func)();
};

struct Song {
	char *s;
	Song *next, *prev;
};

WINDOW *wnd;
int rmax, cmax;
int height;

int mode;

regex_t regex;
int searchwrap;

Song *ssongs[3];
Song *scursors[3];
int soffsets[3];
int snsongs[3];

Song *songs;
int nsongs;
Song *cursor, *oldcursor;
int offset, oldoffset;

Song *undoring;

int quitting, redraw, lock;

int ispaused;
int israndom;

/* Functions */

void quit();
void quitdaemon();

void togglerandom();
void togglepause();
void skip();

void search();
void searchnext();
void searchback();

void up();
void down();
void gotostart();
void gotoend();
void pageup();
void pagedown();

void gotoplaying();

void showplaylists();
void showlist();
void showupcoming();

void playcursor();
void addupcoming();
void addnext();
void removecursor();
void undoremove();

#include "config.h"

/* Don't really need to be seen by config */

void exec(char *args[]);

int len(char *str);
void drawstring(char *string, int r, int c);
void clearrow(int r);
void error(char *mesg);
void message(char *string);

char* getinput(char *start);

void reloadsongs();
void changemode(int m);

void playsong(char *s);

void searchn(int n);

void *update();
void drawbar();
void drawlist();
void draw();

void checkkeys(int key);

char* currentplayingsong();
char* currentplaylist();
void updateispaused();
void updateisrandom();

void exec(char *args[]) {
	if (fork() == 0) {
		execvp(args[0], args);
	}
}

void quit() {
	quitting = 1;
}

void quitdaemon() {
	char *stopdaemon[3]= {"mmusicd", "stop", NULL};
	quit();
	exec(stopdaemon);
}

void togglerandom() {
	char *random[3] = {"mmusicd", "random", NULL};
	exec(random);

	updateisrandom();
}

void togglepause() {
	char *p[3] = {"mmusicd", "pause", NULL};
	exec(p);

	updateispaused();
}

void skip() {
	char *s[3] = {"mmusicd", "skip", NULL};
	exec(s);
}

void pageup() {
	int i = height;
	for (; i > 0 && cursor && cursor->prev; cursor = cursor->prev) i--;
	if (i > 0)
		offset = 0;
	oldoffset = -1;
	redraw = 1;
}

void pagedown() {
	int i = height;
	for (; i > 0 && cursor && cursor->next; cursor = cursor->next) i--;
	if (i > 0) 
		offset = height - 1;
	oldoffset = -1;
	redraw = 1;
}

void gotoend() {
	for (; cursor && cursor->next; cursor = cursor->next);
	offset = height - 1;
	oldoffset = -1;
	redraw = 1;
}

void gotostart() {
	cursor = songs;
	offset = 0;
	oldoffset = -1;
	redraw = 1;
}

void up() {
	if (!cursor->prev) return;
	cursor = cursor->prev;
	offset--;
	if (offset < 0) {
		offset = height - 1;
		oldoffset = -1;
		redraw = 1;
	}
}

void down() {
	if (!cursor->next) return;
	cursor = cursor->next;
	offset++;
	if (offset >= height) {
		offset = 0;
		oldoffset = -1;
		redraw = 1;
	}
}

int len(char *str) {
	char *s;
	for (s = str; *s && *s != '\n'; s++);
	return (s - str);
}

void drawstring(char *string, int r, int c) {
	int or, oc;
	if (r >= rmax || c >= cmax || r < 0 || c < 0)
		return;
	if (string == NULL)
		return error("NULL string.");

	getyx(wnd, or, oc);
	mvaddnstr(r, c, string, -1);
	move(or, oc);
}

void clearrow(int r) {
	int i;
	char *space = malloc(sizeof(char) * cmax);
	for (i = 0; i < cmax; i++) space[i] = ' ';
	drawstring(space, r, 0);
}

void message(char *string) {
	clearrow(rmax - 1);
	drawstring(string, rmax - 1, 0);
}

void error(char *mesg) {
	char *error = malloc(sizeof(char) * (strlen(mesg) + 8));
	sprintf(error, "ERROR: %s", mesg);
	message(error);
	free(error);
	refresh();
	getch();
	clearrow(rmax - 1);
}

void playsong(char *s) {
	char *playargs[4] = {"mmusicd", "play", NULL, NULL};
	playargs[2] = s;
	exec(playargs);
}

void reloadsongs() {
	FILE *p;
	int i, m;
	char *string = malloc(sizeof(char) * 2048);
	Song *s, *o;

	for (m = 0; m < MODE_MAX; m++) {
		if (m == MODE_PLAYLISTS)
			p = popen("mmusicd playlists", "r");
		else if (m == MODE_LIST)
			p = popen("mmusicd playlist-file", "r");
		else if (m == MODE_UPCOMING)
			p = popen("mmusicd upcoming-file", "r");

		if (!p)
			return error("Failed to load list!!");

		s = ssongs[m];

		snsongs[m] = 0;
		while (fgets(string, sizeof(char) * 2048, p)) {
			if (s) {
				s->next = malloc(sizeof(Song));
				if (!s->next)
					error("FAILED TO MALLOC NEXT SONG!");
				s->next->prev = s;
				s = s->next;
			} else {
				s = ssongs[m] = malloc(sizeof(Song));
				s->prev = NULL;
			}
			s->s = malloc(sizeof(char) * 2048);
			if (!s->s)
				error("FAILED TO MALLOC STRING FOR NEXT SONG!");
			strncpy(s->s, string, len(string));
			s->next = NULL;

			snsongs[m]++;
		}

		pclose(p);
	}
}

char* getinput(char *start) {
	int j, c, l, i, s;
	s = len(start);
	l = cmax - s;
	char *buf;

	buf= malloc(sizeof(char) * l);
	for (i = 0; i < l; i++)
		buf[i] = '\0';

	clearrow(rmax - 1);
	curs_set(1);
	move(rmax - 1, 1);
	drawstring(start, rmax - 1, 0); 

	i = 0;
	while (1) {
		c = getch();

		if (c == '\n') {
			break; 
		} else if (c == CTR('g') || c == 27) {
			buf[0] = '\0';
			break;
		} else if (c == KEY_BACKSPACE || c == 127) {
			if (i > 0) {
				for (j = --i; j < len(buf); j++) buf[j] = buf[j + 1];
			} else {
				buf[0] = '\0';
				break;
			}
		} else if (c == KEY_DC) {
			for (j = i; j < len(buf); j++) buf[j] = buf[j + 1];
		} else if (c == KEY_LEFT) {
			if (i > 0)
				i--;
		} else if (c == KEY_RIGHT) {
			if (i < len(buf))
				i++;
		} else if (c >= ' ' && c <= '~') {
			for (j = len(buf); j > i; j--) buf[j] = buf[j - 1];
			buf[i] = c;
			i++;
		}

		clearrow(rmax - 1);
		drawstring(start, rmax - 1, 0); 
		drawstring(buf, rmax - 1, s);
		move(rmax - 1, i + s);
	}

	move(0, 0);
	curs_set(0);

	if (!buf[0])
		message("");

	return buf;
}

void search() {
	int reti;

	char *search = getinput("/");

	if (!search[0])
		return;

	regfree(&regex);
	reti = regcomp(&regex, search, REG_ICASE|REG_EXTENDED);
	if (reti)
		return message("Error initialising regex");

	searchwrap = 0;
	searchn(1);
}

void searchnext() {
	searchn(1);
}

void searchback() {
	searchn(-1);
}

void searchn(int n) {
	int reti;
	Song *s;

	s = cursor;
	while (s) {
		if (n > 0) 
			s = s->next;
		else 
			s = s->prev;
		if (s) {
			if (regexec(&regex, s->s, 0, NULL, 0) == 0)
				break; 
		} else if (searchwrap) {
			if (n > 0)
				s = songs;
			else
				for (s = songs; s && s->next; s = s->next);
			searchwrap = 0;
		}
	}

	if (s) {
		cursor = s;
		offset = height / 2;
		redraw = 1;
	} else {
		message("Wrap?");
		searchwrap = 1;
	}
}

void gotoplaying() {
	char *find = NULL;
	if (mode == MODE_LIST)
		find = currentplayingsong();
	else if (mode == MODE_PLAYLISTS)
		find = currentplaylist();

	cursor = songs;
	while (cursor && strcmp(cursor->s, find) != 0)
		cursor = cursor->next;

	if (cursor && strcmp(cursor->s, find) == 0) {
		offset = height / 2;
	} else {
		cursor = songs;
		message("Could not find current playing song in list!");
	}

	redraw = 1;
}

char *currentplayingsong() {
	Song *s;
	char *playing;

	playing = malloc(sizeof(char) * 2048);

	FILE *playingf = popen("mmusicd playing", "r");

	if (!playingf) {
		error("Failed to get current playing song!");
		return NULL;
	} else {
		fgets(playing, sizeof(char) * 1000, playingf);
		playing[len(playing)] = '\0';
	}

	pclose(playingf);

	return playing;
}

char *currentplaylist() {
	Song *s;
	char *playlist;

	playlist = malloc(sizeof(char) * 2048);

	FILE *playlistf = popen("mmusicd current-playlist", "r");

	if (!playlistf) {
		error("Failed to get current playlist!");
		return NULL;
	} else {
		fgets(playlist, sizeof(char) * 1000, playlistf);
		playlist[len(playlist)] = '\0';
	}

	pclose(playlistf);

	return playlist;
}

void updateispaused() {
	FILE *pausedf = popen("mmusicd ispaused", "r");

	if (!pausedf) {
		error("Failed to determine if daemon is paused!");
	} else {
		if (fgetc(pausedf) == 'y')
			ispaused = 1;
		else
			ispaused = 0;
		pclose(pausedf);
	}
}

void updateisrandom() {
	FILE *randomf = popen("mmusicd israndom", "r");

	if (!randomf) {
		error("Failed to determine if daemon is on random mode!");
	} else {
		if (fgetc(randomf) == 'y')
			israndom = 1;
		else
			israndom = 0;
		pclose(randomf);
	}
}

void drawbar() {
	color_set(2, NULL); 
	clearrow(height);
	drawstring(currentplayingsong(), height, 0);

	if (ispaused)
		drawstring("P", height, cmax - 2);
	if (israndom)
		drawstring("R", height, cmax - 3);

	color_set(1, NULL); 
}

void drawlist() {
	int cursorpos, i;
	Song *s;
	
	if (redraw) {
		redraw = 0;

		/* Find cursorpos in songs. */
		for (cursorpos = 0, s = songs; s && s != cursor; s = s->next)
			cursorpos++;

		if (cursorpos == nsongs) {
			cursorpos = offset = 0;
			cursor = songs;
		}
		
		/* If cursor in last page set offset so last song is at bottom of
		   screen. */
		if (cursorpos > nsongs - height)
			offset = height - (nsongs - cursorpos);
		/* if cursor in first page set offset so first song at top of screen */
		if (cursorpos < height)
			offset = cursorpos;

		for (i = offset - 1, s = cursor->prev; i >= 0 && s;
				s = s->prev, i--)
			drawstring(s->s, i, 1);

		for (i = offset + 1, s = cursor->next; i < height && s;
				s = s->next, i++) 
			drawstring(s->s, i, 1);
	}

	clearrow(oldoffset);
	if (oldcursor)
		drawstring(oldcursor->s, oldoffset, 1);

	color_set(2, NULL);
	clearrow(offset);
	drawstring(cursor->s, offset, 1);
	color_set(1, NULL);

	oldcursor = cursor;
	oldoffset = offset;
}

void draw() {
	while (lock) usleep(10000);
	lock = 1;

	if (redraw) {
		clear();
		drawbar();
	}

	if (!cursor || !songs)
		message("No songs!");
	else
		drawlist();

	lock = 0;
}

void *updateloop() {
	int r, c;
	while (!quitting) {
		getmaxyx(wnd, r, c);
		if (r != rmax || c != cmax) {      
			rmax = r;
			cmax = c;

			height = rmax - 2;

			clear();
		}

		updateispaused();
		updateisrandom();

		redraw = 1;
		draw();

		sleep(5);
	}
}

void checkkeys(int key) {
	Key *k;

	int lkeys = sizeof(keys) / sizeof(keys[0]);
	for (k = keys; k < keys + lkeys; k++) {
		if (key == k->key) {
			k->func();
			return;
		}
	}
}

void playcursor() {
	if (mode == MODE_PLAYLISTS) {
		char *changeplaylistargs[4] = {"mmusicd", "change", cursor->s, NULL};
		exec(changeplaylistargs);
		usleep(100000);
		showlist();
	} else {
		playsong(cursor->s);
		if (cursor->next) {
			cursor = cursor->next;
			redraw = 1;
			offset++;
		}
	}
}

void addupcoming() {
	Song *s, *n;

	if (mode != MODE_LIST && mode != MODE_UPCOMING)
		return;

	char *args[5] = {"mmusicd", "add", "upcoming", NULL, NULL};
	args[3] = cursor->s;
	exec(args);	

	n = malloc(sizeof(Song));
	n->next = n->prev = NULL;
	n->s = malloc(sizeof(char) * 2048);
	strncpy(n->s, cursor->s, len(cursor->s));

	for (s = ssongs[MODE_UPCOMING]; s && s->next; s = s->next);
	if (s) {
		s->next = n;
		n->prev = s;
	} else {
		ssongs[MODE_UPCOMING] = n;
	}

	snsongs[MODE_UPCOMING]++;

	if (cursor->next) {
		cursor = cursor->next;
		offset++;
	}

	redraw = 1;
}

void addnext() {
	if (mode != MODE_LIST && mode != MODE_UPCOMING)
		return;

	char *args[5] = {"mmusicd", "add", "next", NULL, NULL};
	args[3] = cursor->s;
	exec(args);

	if (cursor->next) {
		cursor = cursor->next;
		redraw = 1;
		offset++;
	}
}

void removecursor() {
	Song *s;
	char *removeargs[5] = {"mmusicd", "remove", NULL, NULL, NULL};

	removeargs[3] = cursor->s;
	if (mode == MODE_LIST) {
		removeargs[2] = currentplaylist();
	} else if (mode == MODE_UPCOMING) {
		removeargs[2] = "upcoming";
	} else 
		return;

	exec(removeargs);	

	if (cursor->prev)
		cursor->prev->next = cursor->next;
	if (cursor->next)
		cursor->next->prev = cursor->prev;

	s = cursor;

	cursor = s->next;
	if (!cursor)
		cursor = s->prev;

	oldcursor = NULL;
	redraw = 1;

	s->next = undoring;
	undoring = s;
}

void undoremove() {
	Song *s, *n;
	char *addargs[5] = {"mmusicd", "add", NULL, NULL, NULL};

	if (!undoring) return;

	addargs[3] = undoring->s;
	if (mode == MODE_LIST) {
		addargs[2] = currentplaylist();
	} else if (mode == MODE_UPCOMING) {
		addargs[2] = "upcoming";
	} else 
		return;

	exec(addargs);

	s = undoring;
	undoring = undoring->next;
	if (s->prev) {
		n = s->prev->next;
		s->prev->next = s;
		s->next = n;
	} else {
		s->next = songs;
		songs = s;
	}

	cursor = s;
	redraw = 1;
}

void changemode(int m) {
	ssongs[mode] = songs;
	scursors[mode] = cursor;
	soffsets[mode] = offset;
	snsongs[mode] = nsongs;
	mode = m;
	songs = ssongs[mode];
	cursor = scursors[mode];
	offset = soffsets[mode];
	nsongs = snsongs[mode];

	if (!cursor)
		cursor = songs;

	oldcursor = cursor;
	oldoffset = -1;
	redraw = 1;
}

void showplaylists() {
	changemode(MODE_PLAYLISTS);
}

void showlist() {
	changemode(MODE_LIST);
}

void showupcoming() {
	changemode(MODE_UPCOMING);
}

int main(int argc, char *argv[]) {
	int d, i;
	pthread_t pth;

	setlocale(LC_ALL, "");
	wnd = initscr();
	cbreak();
	noecho();
	start_color();
	curs_set(0);
	keypad(wnd, TRUE);

	clear();

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_BLUE);

	regcomp(&regex, ".", REG_ICASE|REG_EXTENDED);

	for (i = 0; i < 3; i++) {
		ssongs[i] = NULL;
		scursors[i] = NULL;
		soffsets[i] = 0;
		snsongs[i] = 0;
	}

	songs = cursor = undoring = NULL;
	mode = MODE_PLAYLISTS;

	reloadsongs();
	
	songs = ssongs[mode];
	cursor = scursors[mode];
	offset = soffsets[mode];
	nsongs = snsongs[mode];

	showplaylists();
	gotoplaying();

	pthread_create(&pth, NULL, updateloop, "updater");

	usleep(100000);

	quitting = 0;
	while (1) {
		clearrow(rmax - 1);

		checkkeys(d);
		if (quitting)
			break;

		draw();

		refresh();
		d = getch();
	}

	pthread_cancel(pth);

	endwin();
}
