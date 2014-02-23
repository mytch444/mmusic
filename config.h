/*
  This is the config file for mmusic.
 */

#define CTR(a)  (a - 'a' + 1)

static Key keys[] = {
  { 'q',    quit },
  { 'Q',    quitdaemon },
  { 'p',    pause },
  { 'k',    up },
  { 'j',    down },
  { '\n',   playcursor },
  { 'a',    addcursor },
  { 'A',    addnext },
  { 'S',    shuffle },
  { 's',    gotoplaying },
  { 'H',    gotostart },
  { 'L',    gotoend },
  { 'l',    skip },
  { 'h',    prev },
  { '/',    search },
  { 'n',    searchnext },
  { 'N',    searchback },
  { '1',    modeone },
  { '2',    modetwo },
  { 'd',    removecursor },
  
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
