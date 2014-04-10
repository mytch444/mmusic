#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>

int MODE_LIST               = 1;
int MODE_UPCOMING           = 2;

char mmusiccommand[256]     = "mmusicd";
char *stopdaemoncommand     = "mmusicd stop";

char *pausecommand          = "mmusicd pause";
char *skipcommand           = "mmusicd skip";
char *playcommand           = "mmusicd play \"%s\"";

char *playingcommand        = "mmusicd playing";
char *ispausedcommand       = "mmusicd paused";

char *listmusiccommand      = "mmusicd list";

char *playlistscommand      = "mmusicd playlists";

typedef struct {
    int key;
    void (*func)();
} Key;

WINDOW *wnd;
int rmax, cmax;
char **songs;
int lines;

int *locations;
int nlocations;
int clocation;

int offset, oldoffset;
int cursor, oldcursor;
int quitting;
int ispaused;

void quit();
void quitdaemon();

void pause();
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

void playcursor();

void gotoplaying();

void center();

#include "config.h"

// Don't really need to be seen by config
int LEN(const char *str);
void drawstring(char *string, int r, int c);
void drawfullstring(char *string, int r, int c);
void clear_row_row(int r);
void error();

char* get_string(char *start);

int locsong(char *song);
void loadsongs(char *list);

void playsong(char *s);

void searchn(int n);

void addsong(char *song);

void killdaemon();

void gotosong(char *song);
void gotopos(int p);

void *update();
void drawbar();

void checkkeys(int key);

char* currentplayingsong();
void updateispaused();

void quit() {
    quitting = 1;
}

void quitdaemon() {
    quit();
    killdaemon();
}

void killdaemon() {
    system(stopdaemoncommand);
}

void playcursor() {
    playsong(songs[offset + cursor++]);
}

void pageup() {
    cursor -= rmax - 2;
}

void pagedown() {
    cursor += rmax - 2;
}

void gotoend() {
    gotopos(lines - 1);
}

void gotostart() {
    gotopos(0);
}

void up() {
    cursor--;
}

void down() {
    cursor++;
}

void center() {
    int old = cursor;
    cursor = rmax / 2;
    offset += old - cursor;
}

int LEN(const char *str) {
    const char *s;
    for (s = str; *s && *s != '\n'; s++);
    return (s - str);
}

void drawstring(char *string, int r, int c) {
    int or, oc;
    if (r > rmax || c > cmax || r < 0 || c < 0)
        return;
    getyx(wnd, or, oc);
    mvaddnstr(r, c, string, cmax - c);
    move(or, oc);
}

void drawfullstring(char *string, int r, int c) {
    drawstring(string, r, c);
    char space[cmax - LEN(string) + 1];
    int i;
    for (i = 0; i < cmax - LEN(string); i++) space[i] = ' ';
    space[i] = '\0';

    drawstring(space, r, c + LEN(string));
}

void clear_row(int r) {
    drawfullstring(" ", r, 0);
}

void error() {
    clear_row(rmax - 1);
    drawstring("An error occured", rmax - 1, 0);
    getch();
    clear_row(rmax - 1);
}

void loadsongs(char *list) {
    FILE* p;

    char linesc[1024];
    sprintf(linesc, "%s | wc -l", list);
    p = popen(linesc, "r");
    if (!p) {
        error();
        return;
    }

    char buf[12];
    fgets(buf, sizeof(char) * 12, p);
    pclose(p);

    lines = atoi(buf); 
    if (lines < 1) lines = 1;

    p = popen(list, "r");
    if (!p) {
        error();
        return;
    }

    int i = 0; 
    songs = (char**) malloc(lines * 1024 * sizeof(char));
    while (i < lines && fgets((songs[i] = (char*) malloc(1024 * sizeof(char))), sizeof(char) * 1024, p)) {
        songs[i][LEN(songs[i])] = '\0';
        i++;
    }

    pclose(p);
}

char* get_string(char *start) {
    int j, c, l, i;
    l = 1024;
    i = 0;
    char *buf;
    
    buf= malloc(sizeof(char) * l);
    buf[0] = '\0';

    clear_row(rmax - 1);
    curs_set(1);
    move(rmax - 1, 1);
    drawstring(start, rmax - 1, 0); 
    
    while (1) {
        c = getch();

        if (c == '\n') {
            break; 
        } else if (c == CTR('g')) {
            buf[0] = '\0';
            break;
        } else if (c == KEY_BACKSPACE || c == KEY_DC || c == 127) {
            if (i > 0) {
                i--;
                for (j = i; j < LEN(buf); j++) buf[j] = buf[j + 1];
            }
        } else if (c == KEY_LEFT) {
            if (i > 0)
                i--;
        } else if (c == KEY_RIGHT) {
            if (i < LEN(buf))
                i++;
        } else {
            for (j = LEN(buf); j > i; j--) buf[j] = buf[j - 1];
            buf[i] = c;
            i++;
        }

        clear_row(rmax - 1);
        drawstring(start, rmax - 1, 0); 
        drawstring(buf, rmax - 1, LEN(start));
        move(rmax - 1, i + LEN(start));
    }

    move(0, 0);
    curs_set(0);
    return buf;
}

void playsong(char *s) {
    char buf[512] = {'\0'};
    sprintf(buf, playcommand, s);
    system(buf);
    drawbar();
}

void search() {
    int i, results;

    char *search = get_string("/");

    regex_t regex;
    int reti;

    reti = regcomp(&regex, search, REG_ICASE|REG_EXTENDED);
    if (reti) {
        clear_row(rmax - 1);
        drawstring("Error initialising regex", rmax -1, 0);
        return;
    }

    for (i = 0; i < lines; i++) {
        reti = regexec(&regex, songs[i], 0, NULL, 0);

        if (!reti)
            results++;
    }

    locations = malloc(results * sizeof(int));
    clocation = 0;
    nlocations = results;
    for (i = 0; i < lines; i++) {
        reti = regexec(&regex, songs[i], 0, NULL, 0);
        
        if (!reti) {
            locations[clocation] = locsong(songs[i]);
            clocation++;
        }
    }


    regfree(&regex);

    searchn(1);
}

void searchnext() {
    searchn(1);
}

void searchback() {
    searchn(-1);
}

void searchn(int n) {
    if (!locations) {
        clear_row(rmax - 1);
        drawstring("It may help if you search first", rmax - 1, 0);
        return;
    }

    clocation += n;

    if (clocation > nlocations - 1) clocation = 0;
    if (clocation < 0) clocation = nlocations - 1;

    gotopos(locations[clocation]);
    
    char buf[128];
    sprintf(buf, "%i of %i", clocation + 1, nlocations);
    clear_row(rmax - 1);
    drawstring(buf, rmax - 1, 0);
}

void gotopos(int loc) {
    if (loc == -1) {
        drawstring("I cant go to pos -1", rmax - 1, 0);
        return;
    }
    
    if (loc < rmax - 2) {
        offset = 0;
    } else if (loc >= lines - (rmax - 2)) {
        offset = lines - (rmax - 2);
    } else {
        offset = loc - (rmax - 2) / 2;
    }

    cursor = loc - offset;
}

void gotosong(char *song) {
    gotopos(locsong(song));
}

int locsong(char *song) {
    int i;
    for (i = 0; i < lines; i++) {
        if (strcmp(song, songs[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void gotoplaying() {
    char *playing = currentplayingsong(playing);
    gotosong(playing);
}

char* currentplayingsong() {
    char *playing;
    playing = malloc(sizeof(char) * 2048);

    FILE *playingf = popen(playingcommand, "r");

    if (!playingf) {
        error();
    } else {
        fgets(playing, sizeof(char) * (cmax - 2), playingf);
        playing[LEN(playing)] = '\0';
    }

    pclose(playingf);

    return playing; 
}

void updateispaused() {
    char *paused;
    paused = malloc(sizeof(char) * 3);

    FILE *pausedf = popen(ispausedcommand, "r");

    if (!pausedf) {
        error();
    } else {
        fgets(paused, sizeof(char) * 3, pausedf);
        pclose(pausedf);
        if (paused[0] == 'y')
            ispaused = 1;
        else
            ispaused = 0;
    }
}

void pause() {
    system(pausecommand);
    drawbar();
}

void skip() {
    system(skipcommand);
    drawbar();
}

void drawbar() {
    if (quitting) return;

    char *playing = currentplayingsong(playing);

    color_set(2, NULL); 
    drawfullstring(playing, rmax - 2, 0);

    if (ispaused)
        drawstring("P", rmax - 2, cmax - 2);

    color_set(1, NULL); 
}

void drawlist() {
    int i;
    for (i = offset; songs[i] && i - offset < rmax - 2; i++) {
        clear_row(i - offset); 
        drawstring(songs[i], i - offset, 2);
    }
}

void *updateloop() {
    int r, c;
    while (!quitting) {
        getmaxyx(wnd, r, c);
        if (r != rmax || c != cmax) {      
            rmax = r;
            cmax = c;

            clear();
            drawlist();
            drawbar();
        }

        updateispaused();

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

int main(int argc, char *argv[]) {
    int d;

    wnd = initscr();
    cbreak();
    noecho();
    start_color();
    curs_set(0);
    keypad(wnd, TRUE);

    loadsongs(listmusiccommand);

    oldoffset = -1;
    oldcursor = 0;
    offset = 0;
    cursor = 0;

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_BLUE);

    pthread_t pth;
    pthread_create(&pth, NULL, updateloop, "updater");

    clear();

    gotoplaying();

    quitting = 0;
    while (1) {
        clear_row(rmax - 1);

        checkkeys(d);
        if (quitting) {
            break;
        }

        if (offset < 0) {
            offset = 0;
            cursor = 0;
        }

        for (; cursor < 0; ) {
            if (offset <= 0) {
                offset = 0;
                cursor = 0;
            } else {
                cursor = rmax - 2 + cursor;
                offset -= rmax - 2;
                if (offset < 0) {
                    offset = 0;
                }
            }
        }
       
        for (; cursor > rmax - 3; ) {
            if (offset >= lines - (rmax - 2)) {
                offset = lines - (rmax - 2);
                cursor = rmax - 3;
            } else {
                cursor = cursor - (rmax - 2);
                offset += rmax - 2;
                if (offset > lines - (rmax - 2))
                    offset = lines - (rmax - 2);
            }
        }
      
        if (cursor > lines - 1)
            cursor = lines - 1;

        if (oldoffset != offset)
            drawlist();

        if (offset == oldoffset) {
            clear_row(oldcursor);
            drawfullstring(songs[offset + oldcursor], oldcursor, 2);
        }

        oldoffset = offset;
        oldcursor = cursor;

        color_set(2, NULL); 
        drawfullstring(songs[offset + cursor], cursor, 2);
        drawstring("= ", cursor, 0);
        color_set(1, NULL); 

        drawbar();

        refresh();

        updateispaused();
        d = getch();
    }

    pthread_cancel(pth);

    endwin();
}
