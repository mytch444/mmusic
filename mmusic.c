#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *pausecommand          = "mmusicd pause";
char *skipcommand           = "mmusicd skip";
char *playcommand           = "mmusicd play \"%s\"";
char *addupcomingcommand    = "mmusicd add upcoming file \"%s\"";
char *playingcommand        = "mmusicd playing";
char *linescommand          = "%s | wc -l";
char *startplayingcommand   = "if [ -z \"$(mmusicd playing)\" ]; then mmusicd; fi";
char *listmusiclist         = "cat $(mmusicd list)";
char *listmusicupcoming     = "cat $(mmusicd upcoming)";

int rmax, cmax;
char **songs;
int lines;
char *file;
int listcursor, listoffset;
int upcomingcursor, upcomingoffset;
int currentmode;
int *locations;
int nlocations;
int clocations;

int offset, oldoffset;
int cursor, oldcursor;

int LEN(const char *str);
void draw(char dc, int r, int c);
void drawstring(char *string, int r, int c);
void drawfullstring(char *string, int r, int c);
void clearrow(int r);
void error();
void loadsongs();
char* getcommand(char *buf, int l, char start);
void playsong(char *s);
int locsong(char *song);
void search();
void searchn(int n);
void addsong(char *song);
void modeone();
void modetwo();
void startplaying();

int LEN(const char *str) {
    const char *s;
    for (s = str; *s && *s != '\n'; s++);
    return (s - str);
}

void draw(char dc, int r, int c) {
    if (r > rmax || c > cmax) return;
    move(r, c);
    delch();
    insch(dc);
    refresh();
}

void drawstring(char *string, int r, int c) {
    int i;
    for (i = 0; i < LEN(string); i++) {
        draw(string[i], r, c + i);
    }
}

void drawfullstring(char *string, int r, int c) {
    int i;
    drawstring(string, r, c);
    for (i = LEN(string); i < cmax; i++) draw(' ', r, c + i);
}

void clearrow(int r) {
    if (r > rmax) return;
    int c;
    for (c = 0; c < cmax; c++) {
        move(r, c);
        delch();
    }
    refresh();
}

void error() {
    clearrow(rmax - 1);
    drawstring("An error occured", rmax - 1, 0);
    getch();
    clearrow(rmax - 1);
}

void loadsongs() {
    int i;
    FILE* p;

    char linesc[256];
    sprintf(linesc, linescommand, file);
    p = popen(linesc, "r");
    if (!p) {
        error();
        return;
    }

    char buf[12];
    fgets(buf, sizeof(char) * 12, p);
    lines = atoi(buf); 
    if (lines < 1) lines = 1;

    pclose(p);

    songs = (char**) malloc(lines * 512 * sizeof(char));

    for (i = 0; i < lines; i++) {
        songs[i] = (char*) malloc(512 * sizeof(char));
    }

    p = popen(file, "r");
    if (!p) {
        error();
        return;
    }

    i = 0; 
    while (i < lines && fgets(songs[i], sizeof(char) * 512, p)) {
        songs[i][LEN(songs[i])] = '\0';
        i++;
    }

    pclose(p);
}

char* getcommand(char *buf, int l, char start) {
    draw('/', rmax - 1, 0); 
    int i = 0;
    char c;
    while (i < l) {
        c = getch();

        if (c == '\n') {
            break; 
        } else if ((c == KEY_BACKSPACE || c == KEY_DC || c == 127) && i > 0) {
            i--;
            buf[i] = '\0';
        } else {
            buf[i] = c;
            i++;
        }

        clearrow(rmax - 1);
        draw(start, rmax - 1, 0); 
        drawstring(buf, rmax - 1, 1);
        move(rmax - 1, i + 1);
    }
    return buf;
}

void playsong(char *s) {
    char buf[512] = {'\0'};
    sprintf(buf, playcommand, s);
    system(buf);
}

int locsong(char *song) {
    int i;
    for (i = 0; i < lines; i++) {
        if (strcmp(song, songs[i]) == 0) {
            return i;
        }
    }
}

void search() {
    int i, j;

    char search[128] = {'\0'};
    *search = *getcommand(search, 128, '/');
    int results; 

    char linesb[1024] = {'\0'};

    sprintf(linesb, "%s | grep -ie \"%s\" | wc -l", file, search);

    FILE *result = popen(linesb, "r");
    if (!result) {
        error();
        return;
    }

    char linesrb[8] = {'\0'};
    fgets(linesrb, sizeof(char) * 8, result);
    results = atoi(linesrb);
    pclose(result);

    char matchs[results][256];

    char resultsb[1024] = {'\0'}; 
    sprintf(resultsb, "%s | grep -ie \"%s\"", file, search);

    result = popen(resultsb, "r");
    if (!result) {
        error();
        return;
    }

    i = 0;
    while (i < results && fgets(matchs[i], sizeof(char) * 256, result)) {
        matchs[i][LEN(matchs[i])] = '\0'; 
        i++;
    }
    pclose(result);

    locations = malloc(results * sizeof(int));
    nlocations = results;
    clocations = -1;

    j = 0;
    for (i = 0; i < results; i++) {
        locations[j++] = locsong(matchs[i]);
    }
    
    searchn(1);
}

void addsong(char *song) {
    char buf[512] = {'\0'};
    sprintf(buf, addupcomingcommand, song);
    system(buf);
}

void startplaying() {
    system(startplayingcommand);
    clearrow(rmax - 1);
    drawstring(startplayingcommand, rmax - 1, 0);
}

void searchn(int n) {
    clocations += n;

    if (clocations > nlocations - 1) clocations = 0;
    if (clocations < 0) clocations = nlocations - 1;

    cursor = rmax / 2;
    offset = locations[clocations] - rmax / 2;

    if (locations[clocations] < rmax - 3) {
        cursor = locations[clocations];
        offset = 0;
    } else if (locations[clocations] > lines - rmax - 3) {
        cursor = locations[clocations];
        offset = lines - rmax - 3;
    }

    char buf[128];
    sprintf(buf, "%i of %i", clocations + 1, nlocations);
    clearrow(rmax - 1);
    drawstring(buf, rmax - 1, 0);
}

void modeone() {
    if (currentmode == 1) return;
    currentmode = 1; 
    upcomingcursor = cursor;
    upcomingoffset = offset;
    cursor = listcursor;
    offset = listoffset;
    oldoffset = -1;
    oldcursor = cursor;
    file = listmusiclist;
    loadsongs();
}

void modetwo() {
    if (currentmode == 2) return;
    currentmode = 2;
    listcursor = cursor;
    listoffset = offset;
    cursor = upcomingcursor;
    offset = upcomingoffset;
    oldoffset = -1;
    oldcursor = cursor;
    file = listmusicupcoming;
    loadsongs();
}

int main(int argc, char *argv[]) {
    WINDOW *wnd;
    int j;
    char d;

    wnd = initscr();
    cbreak();
    noecho();
    start_color();
    getmaxyx(wnd, rmax, cmax);
    clear();
    refresh();

    file = listmusiclist;
    currentmode = 1;

    loadsongs();

    offset = 0;
    oldoffset = -1;
    cursor = 0;
    oldcursor = -1;

    listcursor = cursor;
    listoffset = offset;
    upcomingcursor = cursor;
    upcomingoffset = offset;

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_BLUE);

    startplaying();

    while (1) {
        if (d == 'q') break;
        if (d == 'p') system(pausecommand); 
        if (d == 'j') cursor++;
        if (d == 'k') cursor--;
        if (d == 'H') offset = cursor = 0;
        if (d == 'L') offset = cursor = -1; 
        if (d == 'J') offset += rmax - 2;
        if (d == 'K') offset -= rmax - 2;
        if (d == '\n') playsong(songs[offset + cursor++]);
        if (d == 'a') addsong(songs[offset + cursor++]);
        if (d == 'l') system(skipcommand);
        if (d == '/') search(); 
        if (d == 'n') searchn(1);
        if (d == 'N') searchn(-1);
        if (d == '1') modeone();
        if (d == '2') modetwo();

        if (cursor < 0) {
            if (rmax - 3 > lines) {
                cursor = lines - 1;
                offset = 0;
            } else {
                cursor = rmax - 3;
                offset -= rmax;
            }
        }

        if (cursor > rmax - 3 || offset + cursor > lines - 1) {
            cursor = 0;
            offset += rmax;
        }

        if (offset < 0) {
            offset = lines - (rmax - 2);
            if (offset < 0) offset = 0;
        }

        if (offset > lines - (rmax - 2)) {
            offset = 0;
        }

        if (oldoffset != offset) {
            oldcursor = -1;
            clear();
            int j;
            for (j = offset; songs[j] && j - offset < rmax - 2; j++) {
                drawstring(songs[j], j - offset, 2);
            }
        }
        oldoffset = offset;

        color_set(2, NULL); 

        char buf[256] = {'\0'};

        sprintf(buf, "= %s", songs[offset + cursor]);
        drawfullstring(buf, cursor, 0);

        FILE *playingf = popen(playingcommand, "r");

        char playing[256];
        if (!playingf) error(); 
        else {
            fgets(playing, sizeof playing, playingf);
            drawfullstring(playing, rmax - 2, 0);
        }

        color_set(1, NULL); 

        if (oldcursor != -1) {
            sprintf(buf, "  %s", songs[offset + oldcursor]);
            drawfullstring(buf, oldcursor, 0);
        }

        oldcursor = cursor;

        d = getch();
    }

    endwin();
}
