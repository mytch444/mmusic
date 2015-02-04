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

typedef struct {
	int key;
	void (*func)();
} Key;

typedef struct Song Song;

struct Song {
	char *s;
	Song *next, *prev;
};

WINDOW *wnd;
int rmax, cmax;
int height;

int islocked;
int mode;

Song *songs;
int nsongs;

regex_t regex;
int searchwrap;

Song *cursors[3];
int offsets[3];
Song *cursor, *oldcursor;
int offset, oldoffset;
int redraw;

int quitting;

int ispaused;
int israndom;

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

#include "config.h"

// Don't really need to be seen by config

void lock();
void unlock();

void exec(char *args[]);

int len(char *str);
void drawstring(char *string, int r, int c);
void drawfullstring(char *string, int r, int c);
void clearrow(int r);
void error(char *mesg);
void message(char *string);

char* getinput(char *start);

void loadsongs();

void playsong(char *s);

void searchn(int n);

void *update();
void drawbar();
void drawlist();

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

void drawfullstring(char *string, int r, int c) {
	drawstring(string, r, c);

	int l = cmax - c - len(string);
	if (l < 1) return;

	char space[l];
	int i;
	for (i = 0; i < l; i++) space[i] = ' ';
	space[i] = '\0';

	drawstring(space, r, c + len(string));
}

void clearrow(int r) {
	drawfullstring(" ", r, 0);
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
	getch();
	clearrow(rmax - 1);
}

void playsong(char *s) {
	char *playargs[4] = {"mmusicd", "play", NULL, NULL};
	playargs[2] = s;
	exec(playargs);
}

void lock() {
	while (islocked)
		usleep(10000);
	islocked = 1;
}

void unlock() {
	islocked = 0;
}	

int numsongs(char *list) {
	FILE *p;
	int l;

	char linesc[1024];
	sprintf(linesc, "%s | wc -l", list);
	p = popen(linesc, "r");
	if (!p) {
		error("Failed to get line count of playlist!");
		return -1;
	}

	char buf[32];
	fgets(buf, sizeof(char) * 32, p);
	pclose(p);

	l = atoi(buf); 

	return l;
}

void loadsongs() {
	FILE *p;
	int i;
	char *filename, *string;
	Song *s, *o;

	s = songs;
	while (s) {
		o = s;
		s = s->next;
	//	free(o);
	}

	if (mode == MODE_PLAYLISTS)
		p = popen("mmusicd playlists", "r");
	else if (mode == MODE_LIST)
		p = popen("mmusicd playlist-file", "r");
	else if (mode == MODE_UPCOMING)
		p = popen("mmusicd upcoming-file", "r");

	if (!p)
		return error("Failed to load list!!");

	songs = malloc(sizeof(Song));
	songs->s = "This file is empty";
	songs->next = NULL;
	s = songs;

	string = malloc(sizeof(char) * 2048);

	nsongs = 0;
	while (fgets(string, sizeof(char) * 2048, p)) {
		s->next = malloc(sizeof(Song));
		s->next->prev = s;
		s = s->next;
		s->s = malloc(sizeof(char) * 2048);
		strncpy(s->s, string, len(string));
		s->next = NULL;
		nsongs++;
	}

	if (songs->next) {
		s = songs->next;
		free(songs);
		songs = s;
		songs->prev = NULL;
	}

	pclose(p);
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
		offset = 0;
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
	while (cursor && cursor->next && strcmp(cursor->s, find) != 0)
		cursor = cursor->next;
	
	offset = 0;
	oldoffset = -1;
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
	lock();

	color_set(2, NULL); 
	drawfullstring(currentplayingsong(), height, 0);
	
	if (ispaused)
		drawstring("P", height, cmax - 2);
	if (israndom)
		drawstring("R", height, cmax - 3);

	color_set(1, NULL); 

	unlock();
}

void drawlist() {
	int i; Song *s;

	lock();

	if (nsongs < height)
		for (offset = -1, s = cursor; s; s = s->prev) offset++;
	if (offset < 0)
		offset = 0;

	if (redraw) {
		for (i = 0; i < height; i++)
			clearrow(i);

		for (i = offset - 1, s = cursor->prev; i >= 0 && s;
				s = s->prev, i--)
			drawstring(s->s, i, 1);

		for (i = offset + 1, s = cursor->next; i < height && s;
				s = s->next, i++) 
			drawstring(s->s, i, 1);
	}

	drawfullstring(oldcursor->s, oldoffset, 1);
	color_set(2, NULL);
	drawfullstring(cursor->s, offset, 1);
	color_set(1, NULL);

	oldcursor = cursor;
	oldoffset = offset;
	redraw = 0;

	unlock();
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
			redraw = 1;
			drawlist();
		}

		updateispaused();
		updateisrandom();

		drawbar();
		refresh();
		sleep(1);
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
	if (mode != MODE_LIST && mode != MODE_UPCOMING)
		return;

	char *args[5] = {"mmusicd", "add", "upcoming", NULL, NULL};
	args[3] = cursor->s;
	exec(args);

	if (cursor->next) {
		cursor = cursor->next;
		redraw = 1;
		offset++;
	}
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

	oldcursor = cursor;
	redraw = 1;

	free(s);
}

void showplaylists() {
	cursors[mode] = cursor;
	offsets[mode] = offset;
	mode = MODE_PLAYLISTS;
	loadsongs();
	offset = offsets[mode];
	cursor =  cursors[mode];
	if (!cursor)
		cursor = songs;
	oldcursor = cursor;
	oldoffset = -1;
	redraw = 1;
}

void showlist() {
	cursors[mode] = cursor;
	offsets[mode] = offset;
	mode = MODE_LIST;
	loadsongs();
	offset = offsets[mode];
	cursor =  cursors[mode];
	if (!cursor)
		cursor = songs;
	oldcursor = cursor;
	oldoffset = -1;
	redraw = 1;
}

void showupcoming() {
	cursors[mode] = cursor;
	offsets[mode] = offset;
	mode = MODE_UPCOMING;
	loadsongs();
	offset = offsets[mode];
	cursor =  cursors[mode];
	if (!cursor)
		cursor = songs;
	oldcursor = cursor;
	oldoffset = -1;
	redraw = 1;
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

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_BLUE);

	islocked = 0;
	regcomp(&regex, ".", REG_ICASE|REG_EXTENDED);

	for (i = 0; i < 3; i++) {
		cursors[i] = NULL;
		offsets[i] = 0;
	}

	songs = cursor = NULL;
	mode = MODE_PLAYLISTS;

	clear();
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

		drawlist();

		refresh();
		d = getch();
	}

	pthread_cancel(pth);

	endwin();
}
