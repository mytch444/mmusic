#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int MODE_LIST               = 1;
int MODE_UPCOMING           = 2;

char *pausecommand          = "mmusicd pause";
char *skipcommand           = "mmusicd skip";
char *prevcommand           = "mmusicd prev";
char *playcommand           = "mmusicd play \"%s\"";
char *addupcomingcommand    = "mmusicd add upcoming file \"%s\"";
char *playingcommand        = "mmusicd playing";
char *linescommand          = "%s | wc -l";
char *startplayingcommand   = "if [ -z \"$(mmusicd playing)\" ]; then mmusicd; fi";
char *listmusiclist         = "cat $(mmusicd list)";
char *listmusicupcoming     = "cat $(mmusicd upcoming)";
char *prefcommandfile       = "cat $(mmusicd preffile)";

char quitkey                = 'q';
char pausekey               = 'p';
char downkey                = 'j';
char upkey                  = 'k';
char startkey               = 'H';
char endkey                 = 'L';
char pagedownkey            = 'J';
char pageupkey              = 'K';
char playsongkey            = '\n';
char addsongkey             = 'a';
char skipkey                = 'l';
char prevkey                = 'h';
char searchkey              = '/';
char searchnextkey          = 'n';
char searchbackkey          = 'N';
char modeonekey             = '1';
char modetwokey             = '2';
char gotoplayingkey         = 's';

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
void gotosong(char *song);

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
    mvaddstr(r, c, string);
}

void drawfullstring(char *string, int r, int c) {
    drawstring(string, r, c);
    char space[cmax - LEN(string) + 1];
    int i;
    for (i = 0; i < cmax - LEN(string); i++) space[i] = ' ';
    space[i] = '\0';

    drawstring(space, r, c + LEN(string));
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
    pclose(p);

    lines = atoi(buf); 
    if (lines < 1) lines = 1;

    p = popen(file, "r");
    if (!p) {
        error();
        return;
    }

    int i = 0; 
    songs = (char**) malloc(lines * 512 * sizeof(char));
    while (i < lines && fgets((songs[i] = (char*) malloc(512 * sizeof(char))), sizeof(char) * 512, p)) {
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

    return -1;
}

void search() {
    int i, results;

    char search[128] = {'\0'};
    *search = *getcommand(search, 128, '/');

    char linesb[512] = {'\0'};

    sprintf(linesb, "%s | grep -ie \"%s\" | wc -l", file, search);

    FILE *result = popen(linesb, "r");
    if (!result) {
        error();
        return;
    }

    char linesrb[8] = {'\0'};
    fgets(linesrb, sizeof(char) * 8, result);
    pclose(result);
    
    results = atoi(linesrb);

    char resultsb[1024] = {'\0'}; 
    sprintf(resultsb, "%s | grep -ie \"%s\"", file, search);

    result = popen(resultsb, "r");
    if (!result) {
        error();
        return;
    }

    char matchs[results][256];
    i = 0;
    while (i < results && fgets(matchs[i], sizeof(char) * 256, result)) {
        matchs[i][LEN(matchs[i])] = '\0'; 
        i++;
    }
    pclose(result);

    locations = malloc(results * sizeof(int));
    nlocations = results;
    clocations = -1;

    for (i = 0; i < results; i++) {
        locations[i] = locsong(matchs[i]);
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

    if (locations[clocations] < rmax - 3) offset = 0;
    else if (locations[clocations] > lines - rmax - 3) offset = lines - rmax - 3;
    else offset = locations[clocations] - rmax / 2;

    cursor = locations[clocations] - offset;

    char buf[128];
    sprintf(buf, "%i of %i", clocations + 1, nlocations);
    clearrow(rmax - 1);
    drawstring(buf, rmax - 1, 0);
}

void gotosong(char *song) {
    int loc = locsong(song);

    if (loc < rmax - 3) {
        offset = 0;
    } else if (loc > lines - (rmax - 3)) {
        offset = lines - (rmax - 3);
    } else {
        offset = loc - (rmax - 3) / 2;
    }

    cursor = loc - offset;
}

void modeone() {
    if (currentmode == MODE_LIST) return;
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
    if (currentmode == MODE_UPCOMING) return;
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

char* currentplayingsong(char playing[256]) {
    FILE *playingf = popen(playingcommand, "r");

    if (!playingf) error(); 
    else {
        fgets(playing, sizeof(char) * 256, playingf);
        playing[LEN(playing)] = '\0';
    }

    return playing;
}

void gotoplaying() {
    if (currentmode == MODE_UPCOMING) return; 
    char playing[256] = {'\0'};
    *playing = *currentplayingsong(playing);
    gotosong(playing);
}

void loadpref() {
    FILE  *p;

    p = popen(prefcommandfile, "r");
    if (!p) {
        error();
        return;
    }

    while (1) {
        char line[256] = {'\0'};
        if (!fgets(line, sizeof(char) * 256, p)) break;
        if (line[0] == '#') continue;

        char command[256] = {'\0'};
        char keystring[10] = {'\0'};

        int i;
        for (i = 0; line[i] && line[i] != '='; i++) command[i] = line[i];
        int keystarts = ++i;
        for (; line[i] && line[i] != '\n'; i++) keystring[i - keystarts] = line[i];

        char key = '\0';
        if (keystring[0] == '\\' && keystring[1] == 'n') key = '\n';
        else key = keystring[0];

        if (strcmp(command, "quitkey") == 0)        quitkey = key;
        if (strcmp(command, "pausekey") == 0)       pausekey = key;
        if (strcmp(command, "downkey") == 0)        downkey = key;
        if (strcmp(command, "upkey") == 0)          upkey = key;
        if (strcmp(command, "startkey") == 0)       startkey = key;
        if (strcmp(command, "endkey") == 0)         endkey = key;
        if (strcmp(command, "pagedownkey") == 0)    pagedownkey = key;
        if (strcmp(command, "pageupkey") == 0)      pageupkey = key;
        if (strcmp(command, "playsongkey") == 0)    playsongkey = key;
        if (strcmp(command, "addsongkey") == 0)     addsongkey = key;
        if (strcmp(command, "skipkey") == 0)        skipkey = key;
        if (strcmp(command, "prevkey") == 0)        prevkey = key;
        if (strcmp(command, "searchkey") == 0)      searchkey = key;
        if (strcmp(command, "searchnextkey") == 0)  searchnextkey = key;
        if (strcmp(command, "searchbackkey") == 0)  searchbackkey = key;
        if (strcmp(command, "modeonekey") == 0)     modeonekey = key;
        if (strcmp(command, "modetwokey") == 0)     modetwokey = key;
        if (strcmp(command, "gotoplayingkey") == 0) gotoplayingkey = key;
    }
}

int main(int argc, char *argv[]) {
    WINDOW *wnd;
    int i;
    char d;

    loadpref();

    wnd = initscr();
    cbreak();
    noecho();
    start_color();
    getmaxyx(wnd, rmax, cmax);
    clear();
    refresh();

    file = listmusiclist;

    loadsongs();

    currentmode = MODE_UPCOMING;

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
    gotoplaying();

    while (1) {
        if (d == quitkey) break;
        if (d == pausekey) system(pausecommand); 
        if (d == downkey) cursor++;
        if (d == upkey) cursor--;
        if (d == startkey) offset = cursor = 0;
        if (d == endkey) offset = cursor = -1; 
        if (d == pagedownkey) offset += rmax - 2;
        if (d == pageupkey) offset -= rmax - 2;
        if (d == playsongkey) playsong(songs[offset + cursor++]);
        if (d == addsongkey) addsong(songs[offset + cursor++]);
        if (d == skipkey) system(skipcommand);
        if (d == prevkey) system(prevcommand);
        if (d == searchkey) search(); 
        if (d == searchnextkey) searchn(1);
        if (d == searchbackkey) searchn(-1);
        if (d == modeonekey) modeone();
        if (d == modetwokey) modetwo();
        if (d == gotoplayingkey) gotoplaying();

        if (cursor < 0) {
            cursor = rmax - 3;
            offset -= rmax;
            if (rmax - 3 > lines - 1) cursor = lines - 1;
        }

        if (cursor > rmax - 3) {
            cursor = 0;
            offset += rmax;
        }

        if (cursor > lines - 1) {
            cursor = 0;
        }

        if (offset < 0) {
            offset = lines - (rmax - 2);
            if (offset < 0) offset = 0;
        }

        if (offset > lines - (rmax - 2)) {
            offset = 0;
        }

        if (oldoffset != offset) {
            oldoffset = offset;
            oldcursor = -1;
            clear();
            for (i = offset; songs[i] && i - offset < rmax - 2; i++) {
                drawstring(songs[i], i - offset, 2);
            }
        }

        color_set(2, NULL); 

        char buf[256] = {'\0'};
        sprintf(buf, "= %s", songs[offset + cursor]);
        drawfullstring(buf, cursor, 0);

        char playing[256] = {'\0'};
        *playing = *currentplayingsong(playing);
        drawfullstring(playing, rmax - 2, 0);

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
