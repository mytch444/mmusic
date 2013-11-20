/*
  This is the config file for mmusic.
  If you want to remove a key setting it to '' is probebly
  a better idea than commenting out the line or deleting it.
 */

#define CTR(a)  (a - 'a' + 1)

static Key keys[] = {
  { 'q',    quit },
  { 'p',    pause },
  { 'k',    up },
  { 'j',    down },
  { 'K',    pageup },
  { 'J',    pagedown },
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
  
  { KEY_DOWN,    down },
  { KEY_UP,      up },
  { CTR('n'),    down },
  { CTR('p'),    up },
  { CTR('f'),    pagedown },
  { CTR('b'),    pageup },
};
