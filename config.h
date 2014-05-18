/*
  This is the config file for mmusic.
 */

#define CTR(a)  (a - 'a' + 1)

static Key keys[] = {
  { 'q',    quit },
  { 27,     quit }, // Esc
  { 'Q',    quitdaemon },
  { 'p',    pause },
  { 'k',    up },
  { 'j',    down },
  { '\n',   playcursor },
  { 's',    gotoplaying },
  { 'l',    skip },
  { '/',    search },
  { 'n',    searchnext },
  { 'N',    searchback },

  { 'a',    addupcoming },
  { 'A',    addnext },
  { 'd',    removecursor },

  { '1',    showplaylists },
  { '2',    showlist },
  { '3',    showupcoming },
  
  { KEY_DOWN,    down },
  { KEY_UP,      up },
  { KEY_PPAGE,   pageup },
  { KEY_NPAGE,   pagedown },
  { KEY_END,     gotoend },
  { KEY_HOME,    gotostart },
  { CTR('n'),    down },
  { CTR('p'),    up },
  { CTR('f'),    pagedown },
  { CTR('b'),    pageup },
  { CTR('l'),    center },
};
