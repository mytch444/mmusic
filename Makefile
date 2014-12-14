all: mmusic mmusicd

mmusic: mmusic.c
	gcc -o mmusic mmusic.c -lncursesw -lpthread

clean:
	rm mmusic

install:
	install -Dm 755 mmusic   /usr/local/bin/mmusic
	install -Dm 755 mmusicd  /usr/local/bin/mmusicd

uninstall:
	rm /usr/local/bin/mmusic
	rm /usr/local/bin/mmusicd
