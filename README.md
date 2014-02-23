MMusic
======

A simple music player that uses mpv as a slave to play its music.

Description
-----------

The daemon "mmusicd" is a bash script that has two files it reads from.
"~/.listmusic" which holds paths to music files you want to plays and
"~/.upcomingmusic" which holds paths to music that you want to play next
which will be removed from it once played.

Run "mmusicd --help" for a help menu on what fuctions there are such as
how to add music to the list or upcoming music files.

The ncurses interface "mmusic" is a simple interface that shows the list
and upcoming files and the song currently playing. Key commands can be
changed by editing "config.h" and recompiling it.

"mmusicnd" is a networking daemon that receives commands from the network.
It is not very safe as any command could be run and is so not recomended.
"mmusicn" is a networking interface that sends commands to mmusicnd so than
music can be controlled from another computer.

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
