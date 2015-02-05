/*
 * Set keybindings for mmusic.
 */

#define CTR(a)  (a - 'a' + 1)

static Key keys[] = {
	{ '1',        showplaylists },
	{ '2',        showlist },
	{ '3',        showupcoming },
	
	{ 'p',        togglepause },
	{ 'r',        togglerandom },

	{ 'k',        up },
	{ 'j',        down },
	{ KEY_DOWN,   down },
	{ KEY_UP,     up },

	{ '\n',       playcursor },
	{ 's',        gotoplaying },
	{ 'l',        skip },
	{ '/',        search },
	{ 'n',        searchnext },
	{ 'N',        searchback },

	{ 'a',        addupcoming },
	{ 'A',        addnext },
	{ 'd',        removecursor },
	{ 'u',        undoremove },

	{ CTR('b'),   pageup },
	{ KEY_PPAGE,  pageup },
	{ CTR('f'),   pagedown },
	{ KEY_NPAGE,  pagedown },

	{ 'G',        gotoend },
	{ KEY_END,    gotoend },
	{ 'g',        gotostart },
	{ KEY_HOME,   gotostart },

	{ 'q',        quit },
	{ 'Q',        quitdaemon },
};
