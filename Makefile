all: mmusic mmusicd

mmusic: mmusic.c config.h
	gcc -o mmusic mmusic.c -lncursesw -lpthread

clean:
	rm mmusic

install: all
	install -Dm 755 mmusic   /usr/bin/mmusic
	install -Dm 755 mmusicd  /usr/bin/mmusicd

uninstall:
	rm /usr/bin/mmusic
	rm /usr/bin/mmusicd
