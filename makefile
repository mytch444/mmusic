music: mmusic.c
	gcc -o mmusic mmusic.c -lcurses -lpthread
clean:
	rm mmusic
