MMusic
======

A simple music player that uses mpv as a slave to play its music. Has 
playlists, upcoming songs, and some other features. Works as a daemon and a
curses interface to communicate with it.

Description
-----------

Run "mmusicd help" for a help menu on what functions there are such as how to 
create playlists and populate them.

The ncurses interface "mmusic" is a simple interface that has three modes, 
one that lists playlists, one that lists songs in the current playlist and one
that lists songs in upcoming.

Key commands can be changed by editing "config.h" and recompiling it.

Playlists are stored in ~/.config/mmusic/playlists as file containing a list of 
songs seperated by new lines. Use `mmusicd add playlist [file | dir]`.

Temp files are in /tmp/mmusic-$USER.

Installation
------------

You will need either mpv or to modify the mmusicd file to change what 
command  is used to play a file. Then run

    # make install 

To compile mmusic and install it and mmusicd to /usr/local/bin/

Todo
----

  * Replace mpv with something more suitable.
