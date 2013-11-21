#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

int MODE_LIST               = 1;
int MODE_UPCOMING           = 2;

char mmusiccommand[256]     = "mmusicd";
char *pausecommand          = "%s pause";
char *skipcommand           = "%s skip";
char *prevcommand           = "%s prev";
char *playcommand           = "%s play \"%s\"";
char *addupcomingcommand    = "%s add upcoming file \"%s\"";
char *playingcommand        = "%s playing";
char *linescommand          = "%s | wc -l";
//char *startplayingcommand   = "if [ -z \"$(%s playing)\" ]; then %s; fi";
char *listmusiclist         = "%s list";
char *listmusicupcoming     = "%s upcoming";
char *preffilecommand       = "%s preffile";
char *shufflecommand        = "%s shuffle";
char *removecommand         = "%s remove %s \"%s\"";

typedef struct {
  int key;
  void (*func)();
} Key;

int rmax, cmax;
char **songs;
int lines;
char file[1024];
int listcursor, listoffset;
int upcomingcursor, upcomingoffset;
int currentmode;
int *locations;
int nlocations;
int clocations;
int offset, oldoffset;
int cursor, oldcursor;
int quitting;

int LEN(const char *str);
void drawstring(char *string, int r, int c);
void drawfullstring(char *string, int r, int c);
void clearrow(int r);
void error();

char* getcommand(char *buf, int l, char *start);
int locsong(char *song);
void loadsongs();
void playsong(char *s);
void searchn(int n);
void addsong(char *song);
void startplaying();
void gotosong(char *song);
void setfile(char *f);
void *update();
void drawbar();
void checkkeys(int key);

void quit();
void prev();
void pause();
void skip();
void modeone();
void modetwo();
void search();
void up();
void down();
void gotostart();
void gotoend();
void pageup();
void pagedown();
void playcursor();
void addcursor();
void skip();
void prev();
void searchnext();
void searchback();
void gotoplaying();
void shuffle();
void removecursor();
void updatelist();

#include "config.h"

void quit() {
  quitting = 1;
}

void searchnext() {
  searchn(1);
}

void searchback() {
  searchn(-1);
}

void addcursor() {
  addsong(songs[offset + cursor++]);
}

void playcursor() {
  playsong(songs[offset + cursor++]);
}

void pageup() {
  offset -= rmax - 1;
}

void pagedown() {
  offset += rmax - 1;
}

void gotoend() {
  offset = cursor = -1; 
}

void gotostart() {
  offset = cursor = 0;
}

void up() {
  cursor--;
}

void down() {
  cursor++;
}

int LEN(const char *str) {
  const char *s;
  for (s = str; *s && *s != '\n'; s++);
  return (s - str);
}

void drawstring(char *string, int r, int c) {
  mvaddnstr(r, c, string, cmax - c);
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
  drawfullstring(" ", r, 0);
}

void error() {
  clearrow(rmax - 1);
  drawstring("An error occured", rmax - 1, 0);
  getch();
  clearrow(rmax - 1);
}

void loadsongs() {
  FILE* p;
  
  char linesc[1024];
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

char* getcommand(char *buf, int l, char *start) {
  drawstring(start, rmax - 1, 0); 
  int i = 0;
  int c;
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
    drawstring(start, rmax - 1, 0); 
    drawstring(buf, rmax - 1, LEN(start));
    move(rmax - 1, i + 1 + LEN(start));
  }
  return buf;
}

void playsong(char *s) {
  char buf[512] = {'\0'};
  sprintf(buf, playcommand, mmusiccommand, s);
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
  *search = *getcommand(search, 128, "/");
  
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
  
  char matchs[results][1024];
  i = 0;
  while (i < results && fgets(matchs[i], sizeof(char) * 1024, result)) {
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
  sprintf(buf, addupcomingcommand, mmusiccommand, song);
  system(buf);
}

void startplaying() {
  /*   
       system(startplayingcommand);
       clearrow(rmax - 1);
       drawstring(startplayingcommand, rmax - 1, 0);
  */
}

void searchn(int n) {
  if (!locations) {
    clearrow(rmax - 1);
    drawstring("It may help if you search first", rmax - 1, 0);
    getch();
    clearrow(rmax - 1);
    return;
  }
  
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
  currentmode = MODE_LIST; 
  upcomingcursor = cursor;
  upcomingoffset = offset;
  cursor = listcursor;
  offset = listoffset;
  oldoffset = -1;
  oldcursor = -1;
  setfile(listmusiclist);
  
  updatelist();
}

void modetwo() {
  currentmode = MODE_UPCOMING;
  listcursor = cursor;
  listoffset = offset;
  cursor = upcomingcursor;
  offset = upcomingoffset;
  oldoffset = -1;
  oldcursor = -1;
  setfile(listmusicupcoming);
  
  updatelist();
}

char* currentplayingsong(char playing[1024]) {
  char filename[1024];
  sprintf(filename, playingcommand, mmusiccommand);
  FILE *playingf = popen(filename, "r");
  
  if (!playingf) {
    clearrow(rmax - 1);
    drawstring("Could not get currently playing song for some fucking annoying unknown reason. Thankyou.", rmax - 1, 0);
  } else {
    fgets(playing, sizeof(char) * (cmax - 2), playingf);
    playing[LEN(playing)] = '\0';
  }
  
  pclose(playingf);
  
  return playing;
}

void gotoplaying() {
  if (currentmode == MODE_UPCOMING) return; 
  char playing[1024] = {'\0'};
  *playing = *currentplayingsong(playing);
  gotosong(playing);
}

void shuffle() {
  char shuffletext[1024];
  sprintf(shuffletext, shufflecommand, mmusiccommand);
  system(shuffletext);
  loadsongs();
  oldoffset = -1;
}

void pause() {
  char pausetext[1024];
  sprintf(pausetext, pausecommand, mmusiccommand);
  system(pausetext);
}

void skip() {
  char skiptext[1024];
  sprintf(skiptext, skipcommand, mmusiccommand);
  system(skiptext);
}

void prev() {
  char prevtext[1024];
  sprintf(prevtext, prevcommand, mmusiccommand);
  system(prevtext);
}

void setfile(char *f) {
  sprintf(file, f, mmusiccommand);
}

void drawbar() {
  if (quitting) return;
  
  char playing[1024];
  *playing = *currentplayingsong(playing);
  
  color_set(2, NULL); 
  drawfullstring(playing, rmax - 2, 0);
  color_set(1, NULL); 
}

void *update() {
  while (!quitting) {
    drawbar();
    refresh();
    sleep(3);
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

void removecursor() {
  char buf[1024] = "";
  // I don't think I want to to be able to remove from list
  sprintf(buf, removecommand, mmusiccommand, "upcoming", songs[offset + cursor]);
  system(buf);

  updatelist();
}

void updatelist() {
  oldoffset = -1;
  loadsongs();
  clear();
  drawbar();
}

int main(int argc, char *argv[]) {
  WINDOW *wnd;
  int i;
  int d;
  
  if (argc > 1) {
    char *host = argv[1];
    char *port = argv[2];
    int i;
    for (i = 0; mmusiccommand[i]; i++) mmusiccommand[i] = '\0';
    sprintf(mmusiccommand, "mmusicn %s %s", host, port);
  }
  
  wnd = initscr();
  cbreak();
  noecho();
  start_color();
  getmaxyx(wnd, rmax, cmax);
  curs_set(0);
  keypad(wnd, TRUE);
  clear();
  refresh();
  
  currentmode = MODE_LIST;
  
  setfile(listmusiclist);
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
  
  pthread_t pth;
  pthread_create(&pth, NULL, update, "updater");
  
  //startplaying();
  gotoplaying();

  quitting = 0;
  while (1) {
    checkkeys(d);
    if (quitting) {
      break;
    }
    
    /*
    char str[] = "";
    sprintf(str, "%i %c", d, d);
    clearrow(rmax - 1);
    drawstring(str, rmax - 1, cmax - 10);
    */
    
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
      for (i = offset; songs[i] && i - offset < rmax - 2; i++) {
	clearrow(i - offset); 
	drawstring(songs[i], i - offset, 2);
      }
    }
    
    char buf[1024];
    
    if (oldcursor != -1) {
      clearrow(oldcursor);
      drawfullstring(songs[offset + oldcursor], oldcursor, 2);
    }
    
    oldcursor = cursor;
    
    color_set(2, NULL); 
    drawfullstring(songs[offset + cursor], cursor, 2);
    drawstring("= ", cursor, 0);
    color_set(1, NULL); 
    
    refresh();
    
    d = getch();
  }
  
  pthread_cancel(pth);

  endwin();
}

