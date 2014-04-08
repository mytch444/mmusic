all: mmusic

mmusic: mmusic.c
	gcc -o bin/mmusic mmusic.c -lncursesw -lpthread
	cp mmusicd bin/mmusicd
clean:
	rm bin/*

install:
	install -Dm 755 bin/mmusic   /usr/local/bin/mmusic
	install -Dm 755 bin/mmusicd  /usr/local/bin/mmusicd

uninstall:
	rm /usr/local/bin/mmusic
	rm /usr/local/bin/mmusicd
	rm /usr/local/bin/mmusicn
	rm /usr/local/bin/mmusicnd
