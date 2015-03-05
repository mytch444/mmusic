#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <locale.h>
#include <signal.h>

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
int volume;

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

void increasevolume();
void decreasevolume();

#include "config.h"

/* Don't really need to be seen by config */

void exec(char *args[]);

int len(char *str);
void drawstring(char *string, int r, int c);
void clearrow(int r);
void error(char *mesg);
void message(char *string);

char* getinput(char *start);

void reloadsongs(int m);
void changemode(int m);

void playsong(char *s);

void searchn(int n);

void *update();
void drawbar();
void drawlist();
void draw();

void checkkeys(int key);

Song *popsong(Song *songs, char *s);
char* currentplayingsong();
char* currentplaylist();
void updateispaused();
void updateisrandom();
void updatevolume();

void setvolume(int v);

void exec(char *args[]) {
	if (fork() == 0)
		execvp(args[0], args);
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
	if (!cursor || !cursor->prev) return;
	cursor = cursor->prev;
	offset--;
	if (offset < 0) {
		offset = height - 1;
		oldoffset = -1;
		redraw = 1;
	}
}

void down() {
	if (!cursor || !cursor->next) return;
	cursor = cursor->next;
	offset++;
	if (offset >= height) {
		offset = 0;
		oldoffset = -1;
		redraw = 1;
	}
}

void setvolume(int v) {
	if (v > 100) v = 100;
	if (v < 0) v = 0;
	char *args[4] = {"mmusicd", "set-volume", NULL, NULL};
	args[2] = malloc(sizeof(char) * 4);
	sprintf(args[2], "%i", v);
	exec(args);
}

void updatevolume() {
	FILE *volf = popen("mmusicd get-volume", "r");
	char *volstr  = malloc(sizeof(char) * 4);

	if (!volf) {
		error("Failed to determine if daemon volume!");
	} else {
		fgets(volstr, sizeof(char) * 4, volf);
		pclose(volf);
		volume = atoi(volstr);
	}
}

void increasevolume() {
	updatevolume();
	setvolume(volume + 5);
}

void decreasevolume() {
	updatevolume();
	setvolume(volume - 5);
}

int len(char *str) {
	char *s;
	for (s = str; *s && *s != '\n'; s++);
	return (s - str);
}

void drawnstring(char *string, int r, int c, int max) {
	int l;
	if (!string || r >= rmax || c >= cmax || r < 0 || c < 0)
		return;

	l = len(string);
	if (l > max)
		l = max;
	mvaddnstr(r, c, string, l);
}

void drawstring(char *string, int r, int c) {
	drawnstring(string, r, c, cmax - c);
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
	refresh();
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

void reloadsongs(int m) {
	FILE *p;
	char *string = malloc(sizeof(char) * 2048);
	int newn;
	Song *s, *o, *news;

	if (m == MODE_PLAYLISTS)
		p = popen("mmusicd playlists", "r");
	else if (m == MODE_LIST)
		p = popen("mmusicd playlist-file", "r");
	else if (m == MODE_UPCOMING)
		p = popen("mmusicd upcoming-file", "r");

	if (!p)
		return error("Failed to load list!!");

	s = NULL;
	news = NULL;

	newn = 0;
	while (fgets(string, sizeof(char) * 2048, p)) {
		o = popsong(ssongs[m], string);

		if (o) {
			if (s) {
				s->next = o;
				s->next->prev = s;
				s = s->next;
			} else {
				s = news = o;
				s->prev = NULL;
			}
		} else {
			if (s) {
				s->next = malloc(sizeof(Song));
				s->next->prev = s;
				s = s->next;
			} else  {
				s = news = malloc(sizeof(Song));
				s->prev = NULL;
			}

			s->s = malloc(sizeof(char) * 2048);
			strncpy(s->s, string, len(string));
		}

		s->next = NULL;

		newn++;
	}

	s = ssongs[m];
	while (s) {
		o = s;
		s = s->next;
		free(o);
	}

	snsongs[m] = newn;
	ssongs[m] = news;

	pclose(p);

	if (m == mode) {
		songs = ssongs[mode];
		nsongs = snsongs[mode];
		redraw = 1;
	}
}

Song *popsong(Song *songs, char *s) {
	Song *o;
	for (o = songs; o; o = o->next)
		if (strcmp(o->s, s) == 0)
			break;

	if (o && o->prev)
		o->prev->next = o->next;
	if (o && o->next)
		o->next->prev = o->prev;
	return o;
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
		message("Press n/N again to wrap?");
		searchwrap = 1;
	}
}

void gotoplaying() {
	char *find = NULL;
	if (mode == MODE_LIST)
		find = currentplayingsong();
	else if (mode == MODE_PLAYLISTS)
		find = currentplaylist();

	if (!find)
		return;

	cursor = songs;
	while (cursor && strcmp(cursor->s, find) != 0)
		cursor = cursor->next;

	if (cursor && strcmp(cursor->s, find) == 0) {
		offset = height / 2;
	} else {
		cursor = songs;
		message("Could not find current playing song in list!");
	}

	oldcursor = NULL;

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
	}

	fgets(playing, sizeof(char) * 1000, playingf);
	pclose(playingf);

	if (!playing || len(playing) == 0)
		return NULL;

	playing[len(playing)] = '\0';

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
	}
		
	fgets(playlist, sizeof(char) * 1000, playlistf);
	pclose(playlistf);
	
	if (len(playlist) == 0) {
		message("Not Playing.");
		return NULL;
	}
	
	playlist[len(playlist)] = '\0';

	return playlist;
}

void updateispaused() {
	FILE *pausedf = popen("mmusicd is-paused", "r");

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
	FILE *randomf = popen("mmusicd is-random", "r");

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
	char *current = currentplayingsong();
	char *volstr = malloc(sizeof(char) * 4);

	color_set(2, NULL); 
	clearrow(height);
	if (current) 
		drawnstring(current, height, 0, cmax - 4);

	if (ispaused)
		drawstring("P", height, cmax - 7);
	if (israndom)
		drawstring("R", height, cmax - 6);

	sprintf(volstr, "%i%%", volume);
	drawstring(volstr, height, cmax - strlen(volstr));

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

	if (oldcursor) {
		clearrow(oldoffset);
		drawstring(oldcursor->s, oldoffset, 1);
	}

	color_set(2, NULL);
	clearrow(offset);
	drawstring(cursor->s, offset, 1);
	color_set(1, NULL);

	oldcursor = cursor;
	oldoffset = offset;
}

void draw() {
	int i;

	while (lock) 
		usleep(1000);
	lock = 1;

	if (redraw) {
		for (i = 0; i < height + 1; i++)
			clearrow(i);
		drawbar();
	}

	if (!cursor || !songs)
		message("No songs!");
	else
		drawlist();

	refresh();

	lock = 0;
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

		Song *s, *o;

		s = ssongs[MODE_LIST];
		while (s) {
			o = s;
			s = s->next;
			free(o);
		}

		ssongs[MODE_LIST] = NULL;
		scursors[MODE_LIST] = NULL;
		snsongs[MODE_LIST] = 0;
		soffsets[MODE_LIST] = 0;

		usleep(100000);
		reloadsongs(MODE_LIST);
		showlist();

		gotoplaying();
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
	Song *n;
	if (mode != MODE_LIST && mode != MODE_UPCOMING)
		return;

	char *args[5] = {"mmusicd", "add", "next", NULL, NULL};
	args[3] = cursor->s;
	exec(args);

	n = malloc(sizeof(Song));
	n->prev = NULL;
	n->next = ssongs[MODE_UPCOMING];
	n->s = malloc(sizeof(char) * 2048);
	strncpy(n->s, cursor->s, len(cursor->s));
	ssongs[MODE_UPCOMING] = n;

	snsongs[MODE_UPCOMING]++;

	if (cursor->next) {
		cursor = cursor->next;
		offset++;
	}

	redraw = 1;
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
		n->prev = s;
	} else {
		s->next = songs;
		songs = s;
	}

	cursor = s;
	offset = height / 2;
	redraw = 1;
}

void changemode(int m) {
	scursors[mode] = cursor;
	soffsets[mode] = offset;
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

		reloadsongs(MODE_UPCOMING);

		updateispaused();
		updateisrandom();
		updatevolume();

		redraw = 1;
		draw();

		sleep(2);
	}
}

int main(int argc, char *argv[]) {
	int d, i;
	pthread_t pth;

	setlocale(LC_ALL, "");
	signal(SIGCHLD, SIG_IGN);

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

	songs = cursor = undoring = NULL;
	mode = MODE_PLAYLISTS;

	for (i = 0; i < 3; i++) {
		ssongs[i] = NULL;
		scursors[i] = NULL;
		soffsets[i] = 0;
		snsongs[i] = 0;

		reloadsongs(i);
	}

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
		drawbar();

		refresh();
		d = getch();
	}

	pthread_cancel(pth);

	endwin();
}
