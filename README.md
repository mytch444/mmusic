MMusic
======

A simple music player that uses mpv its music. Has playlists, upcoming songs, 
and some other features. Works as a daemon with a curses interface to 
communicate with it.

Description
-----------

Run "mmusicd help" for a help menu for an idea of what you can do and how to do
 it.

The ncurses interface "mmusic" is a simple interface that has three modes, one 
that lists playlists, one that lists songs in the current playlist and one that
lists songs in upcoming. From here you can search through songs, play, add to 
upcoming, remove from upcoming and playlists, and so on.

Key commands can be changed by editing "config.h" and recompiling it.

Playlists are stored in ~/.config/mmusic/playlists as files containing a list of 
songs seperated by new lines. Use `mmusicd add playlist [file | dir]` to create
a new playlist and populate it with mp3/flac/m4a/ogg files under dir.

Temp files are in /tmp/mmusic-$USER.

Once you've created a playlist try `mmusic`, then select your playlist (hjkl)
by pressing enter. It should now change to a list of your playlist and you 
should here it playing.

Installation
------------

You will need either mpv or to modify the mmusicd file to change what 
command  is used to play a file. Then run

    # make install 

To compile mmusic and install it and mmusicd to /usr/local/bin/

Todo
----

  * Replace mpv with something more suitable.
