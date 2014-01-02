music: mmusic.c
	gcc -o bin/mmusic mmusic.c -lncursesw -lpthread
	cp mmusicd bin/mmusicd
	cp mmusicn bin/mmusin
	cp mmusicnd bin/mmusicnd
clean:
	rm bin/*

install:
	cp bin/* /usr/local/bin/

uninstall:
	rm /usr/local/bin/mmusic
	rm /usr/local/bin/mmusicd
	rm /usr/local/bin/mmusicn
	rm /usr/local/bin/mmusicnd
