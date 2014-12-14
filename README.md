MMusic
======

A simple music player that uses mpv as a slave to play its music. Has playlists,
upcoming songs, and some other features.
Works as a daemon and a curses interface to communicate with it.

Description
-----------

Run "mmusicd help" for a help menu on what functions there are such as how to 
create playlists and populate them.

The ncurses interface "mmusic" is a simple interface that shows the list and 
upcoming files and the song currently playing. Key commands can be changed by 
editing "config.h" and recompiling it.

Installation
------------

You will either need mpv or need to modify the mmusicd file to change what 
command  is used to play a file. Then run

    $ make
    $ make install

This installs the files to /usr/local/bin/

Todo
----

  * Replace mpv with something more suitable.
