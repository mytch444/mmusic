music: mmusic.c
	gcc -o mmusic mmusic.c -lncursesw -lpthread
clean:
	rm mmusic
