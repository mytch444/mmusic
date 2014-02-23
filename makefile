music: mmusic.c
	gcc -o bin/mmusic mmusic.c -lncursesw -lpthread
	cp mmusicd bin/mmusicd
	cp mmusicn bin/mmusicn
	cp mmusicnd bin/mmusicnd
clean:
	rm bin/*

install:
	install -Dm 755 bin/mmusic   /usr/local/bin/mmusic
	install -Dm 755 bin/mmusicd  /usr/local/bin/mmusicd
	install -Dm 755 bin/mmusicn  /usr/local/bin/mmusicn
	install -Dm 755 bin/mmusicnd /usr/local/bin/mmusicnd

uninstall:
	rm /usr/local/bin/mmusic
	rm /usr/local/bin/mmusicd
	rm /usr/local/bin/mmusicn
	rm /usr/local/bin/mmusicnd
