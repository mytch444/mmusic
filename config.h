/*
  This is the config file for mmusic.
 */

#define CTR(a)  (a - 'a' + 1)

static Key keys[] = {
  { 'q',    quit },
  { 'p',    pause },
  { 'k',    up },
  { 'j',    down },
  { '\n',   playcursor },
  { 'a',    addcursor },
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
  { CTR('n'),    down },
  { CTR('p'),    up },
  { CTR('f'),    pagedown },
  { CTR('b'),    pageup },
};
