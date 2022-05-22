.PHONY: build
build: build_dir build/usr/sbin/init build/bin/sh \
	build/usr/sbin/tcp_pty

.PHONY: rawdisk
rawdisk: build
	sudo mount /dev/loop0p1 /home/matt/newsys; \
		sudo cp -R /home/matt/yip_linux/build/* /home/matt/newsys; \
		sleep 1; \
		sudo umount /home/matt/newsys

.PHONY: clean
clean:
	rm -rf build
	rm *.o

.PHONY: build_dir
build_dir:
	mkdir -p build/bin build/dev build/proc build/run \
		build/usr/sbin build/sys

CC=musl-gcc

user_session.o: user_session.h user_session.c
	$(CC) -o user_session.o -c user_session.c

build/usr/sbin/init: init.c user_session.o
	$(CC) -o build/usr/sbin/init init.c user_session.o --static

build/usr/sbin/tcp_pty: tcp_pty.c user_session.o
	$(CC) -o build/usr/sbin/tcp_pty tcp_pty.c user_session.o --static

build/bin/sh: sh.c
	$(CC) -o build/bin/sh sh.c --static
