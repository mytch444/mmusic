MMusic
======

A simple music player that uses mpv as a slave to play its music.

Description
-----------

The daemon "mmusicd" plays music from text files stored at "~/.mmusic/playlists/"
and give you ways to interact with them and the player. Read the help menu.

Run "mmusicd --help" for a help menu on what fuctions there are such as
how to create playlists and populate them.

The ncurses interface "mmusic" is a simple interface that shows the list
and upcoming files and the song currently playing. Key commands can be
changed by editing "config.h" and recompiling it.

Installation
------------

You will need mpv then run the following

    make

To compile, then run
    
    make install

To install the files to /usr/local/bin/

Todo
----

  * Replace mpv with something more suitable.
