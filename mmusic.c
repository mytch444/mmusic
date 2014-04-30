#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>

int MODE_PLAYLIST           = 0;
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
char *upcomingmusiccommand  = "mmusicd upcoming";
char *playlistscommand      = "mmusicd playlists";

char *changeplaylistcommand = "mmusicd change \"%s\"";

char *addupcomingcommand    = "mmusicd add upcoming \"%s\"";
char *addnextcommand        = "mmusicd add next \"%s\"";
char *removecommand         = "mmusicd remove %s %i";

char *currentplaylistcommand = "mmusicd currentplaylist";

typedef struct {
    int key;
    void (*func)();
} Key;

WINDOW *wnd;
int rmax, cmax;
char **songs;
int lines;
int mode;

int *locations;
int nlocations;
int clocation;

int **positions;
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

void showplaylists();
void showlist();
void showupcoming();

void addupcoming();
void addnext();
void removecursor();

#include "config.h"

// Don't really need to be seen by config
int LEN(const char *str);
void drawstring(char *string, int r, int c);
void drawfullstring(char *string, int r, int c);
void clear_row_row(int r);
void error();
void message(char *string);

char*safe_name(char *string);
int char_special(char c);

char* get_string(char *start);

int locsong(char *song);
void loadsongs(char *list);

void playsong(char *s);

void searchn(int n);

void addsong(char *song);

void gotosong(char *song);
void gotopos(int p);

void *update();
void drawbar();

void checkkeys(int key);

char* currentplayingsong();
void updateispaused();
char* currentplaylist();

void quit() {
    quitting = 1;
}

void quitdaemon() {
    quit();
    system(stopdaemoncommand);
}

void playcursor() {
    if (mode == MODE_PLAYLIST) {
        char buf[1024];
        sprintf(buf, changeplaylistcommand, songs[offset + cursor]);
        system(buf);

        showlist();
    } else
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
    if (string == NULL) {
        message("Well how the fuck am I supposed to draw a null string huh, huh!");
        return;
    }
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

void message(char *string) {
    clear_row(rmax - 1);
    drawstring(string, rmax - 1, 0);
}

void error() {
    message("An error occured");
    getch();
    clear_row(rmax - 1);
}

void playsong(char *s) {
    char buf[512];
    sprintf(buf, playcommand, safe_name(s));
    system(buf);
}

void loadsongs(char *list) {
    FILE* p;
    int i;

    for (i = 0; i < lines; i++)
        free(songs[i]);
    if (i > 0)
        free(songs);

    char linesc[1024];
    sprintf(linesc, "%s | wc -l", list);
    p = popen(linesc, "r");
    if (!p)
        return error();

    char buf[12];
    fgets(buf, sizeof(char) * 12, p);
    pclose(p);

    lines = atoi(buf); 
    if (lines < 1) lines = 1;

    p = popen(list, "r");
    if (!p)
        return error();

    songs = malloc(lines * sizeof(char*));
    for (i = 0; i < lines; i++) {
        songs[i] = malloc(1024 * sizeof(char));

        if (!fgets(songs[i], sizeof(char) * 1024, p))
            break;
        
        songs[i][LEN(songs[i])] = '\0'; // Replaces the new line char with null.
    }

    if (locations)
        free(locations);
    locations = malloc(lines * sizeof(int)); // Cant be more matches than the number of songs getting checked right?

    pclose(p);
}

/*
 * Having some problems with this on my computer, it seems to add random characters that cant be displayed
 * and fuck everything up (hold delete and it removes them). But it doesn't do this on my laptop so I'm quite
 * completely baffled. That laptops 64 and the server 32 bit if that makes a difference.
 */
char* get_string(char *start) {
    int j, c, l, i, s;
    s = LEN(start);
    l = cmax - s;
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
        } else if (c == CTR('g') || c == 27) {
            buf[0] = '\0';
            break;
        } else if (c == KEY_BACKSPACE || c == 127) {
            if (i > 0) {
                i--;
                for (j = i; j < LEN(buf); j++) buf[j] = buf[j + 1];
            }
        } else if (c == KEY_DC) {
            for (j = i; j < LEN(buf); j++) buf[j] = buf[j + 1];
        } else if (c == KEY_LEFT) {
            if (i > 0)
                i--;
        } else if (c == KEY_RIGHT) {
            if (i < LEN(buf))
                i++;
        } else if (c >= ' ' && c <= '~') {
            for (j = LEN(buf); j > i; j--) buf[j] = buf[j - 1];
            buf[i] = c;
            i++;
        }

        clear_row(rmax - 1);
        drawstring(start, rmax - 1, 0); 
        drawstring(buf, rmax - 1, s);
        move(rmax - 1, i + s);
    }

    move(0, 0);
    curs_set(0);
    return buf;
}

void search() {
    int i, reti;

    char *search = get_string("/");

    if (search[0] == '\0')
        return message("Done");

    regex_t regex;

    reti = regcomp(&regex, search, REG_ICASE|REG_EXTENDED);
    if (reti)
        return message("Error initialising regex");

    nlocations = 0;
    clocation = -1;
    for (i = 0; i < lines; i++) {
        reti = regexec(&regex, songs[i], 0, NULL, 0);

        if (reti == 0) {
            locations[nlocations] = i;
            nlocations++;
        }
    }

    if (nlocations == 0) 
        return message("No results found");
    
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
    if (!locations || nlocations == 0) 
        return message("It may help if you search first");

    clocation += n;

    if (clocation > nlocations - 1) clocation = 0;
    if (clocation < 0) clocation = nlocations - 1;

    gotopos(locations[clocation]);
    
    char buf[128];
    sprintf(buf, "%i of %i", clocation + 1, nlocations);
    message(buf);
}

void gotopos(int loc) {
    if (loc == -1)
        return message("I cant go to pos -1");
    
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
    char *playing = currentplayingsong();
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

char* currentplaylist() {
    char *playing;
    playing = malloc(sizeof(char) * 2048);

    FILE *playingf = popen(currentplaylistcommand, "r");

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
}

void skip() {
    system(skipcommand);
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
    for (i = 0; i < rmax - 2; i++)
        clear_row(i);
    for (i = offset; i < lines && i - offset < rmax - 2; i++) {
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

            drawlist();
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

void addupcoming() {
    if (mode != MODE_LIST && mode != MODE_UPCOMING)
        return;

    char buf[2048];
    sprintf(buf, addupcomingcommand, safe_name(songs[offset + cursor]));
    system(buf);
    cursor++;
}

void addnext() {
    if (mode != MODE_LIST && mode != MODE_UPCOMING)
        return;

    char buf[2048];
    sprintf(buf, addnextcommand, safe_name(songs[offset + cursor]));
    system(buf);
    cursor++;
}

void removecursor() {
    char buf[2048];
    char *playlist;
    char *list;

    if (mode == MODE_LIST) {
        playlist = currentplaylist();
        list = listmusiccommand;
    } else if (mode == MODE_UPCOMING) {
        playlist = "upcoming";
        list = upcomingmusiccommand;
    }

    sprintf(buf, removecommand, playlist, offset + cursor + 1);
    system(buf);

    loadsongs(list);
    oldoffset = -1;
}

int char_special(char c) {
    return (
            c == '$'
           );
}

char* safe_name(char *plain) {
    char *safe;
    int pl = LEN(plain);
    int l, i, j;
 
    for (i = l = 0; i < pl; i++) {
        if (char_special(plain[i])) l++;
        l++;
    }

    safe = NULL; 
    safe = malloc(sizeof(char) * (l + 1));

    for (i = j = 0; j < l; i++) {
        if (char_special(plain[i]))
            safe[j++] = '\\';
        safe[j++] = plain[i];
    }
    safe[j] = '\0'; 

    return safe;
}

void showplaylists() {
    positions[mode][0] = offset;
    positions[mode][1] = cursor;
    mode = MODE_PLAYLIST;
    loadsongs(playlistscommand);
    offset = oldoffset = positions[mode][0];
    cursor = oldcursor = positions[mode][1];
    clear();
    drawlist();
    drawbar();
}

void showlist() {
    positions[mode][0] = offset;
    positions[mode][1] = cursor;
    mode = MODE_LIST;
    loadsongs(listmusiccommand);
    offset = oldoffset = positions[mode][0];
    cursor = oldcursor = positions[mode][1];
    clear();
    drawlist();
    drawbar();
}

void showupcoming() {
    positions[mode][0] = offset;
    positions[mode][1] = cursor;
    mode = MODE_UPCOMING;
    loadsongs(upcomingmusiccommand);
    offset = oldoffset = positions[mode][0];
    cursor = oldcursor = positions[mode][1];
    clear();
    drawlist();
    drawbar();
}

int main(int argc, char *argv[]) {
    int d, i;

    wnd = initscr();
    cbreak();
    noecho();
    start_color();
    curs_set(0);
    keypad(wnd, TRUE);

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_BLUE);

    locations = NULL;
    nlocations = 0;

    positions = malloc(sizeof(int*) * 3);
    for (i = 0; i < 3; i++) {
        positions[i] = malloc(sizeof(int) * 2);
        positions[i][0] = 0;
        positions[i][1] = 0;
    }

    offset = oldoffset = 0;
    cursor = oldcursor = 0;
    
    clear();
    showplaylists();
    
    pthread_t pth;
    pthread_create(&pth, NULL, updateloop, "updater");

    usleep(100000);

    quitting = 0;
    while (1) {
        updateispaused();
        
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
        d = getch();
    }

    pthread_cancel(pth);

    endwin();
}
