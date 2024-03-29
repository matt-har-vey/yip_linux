.PHONY: build
build: build_dir build/usr/sbin/init build/bin/sh \
	build/usr/sbin/tcp_pty build/usr/bin/login

.PHONY: rawdisk
rawdisk: build
	sudo mount /dev/loop0p1 /home/matt/newsys; \
		sudo cp -R /home/matt/yip_linux/build/* /home/matt/newsys; \
		sleep 1; \
		sudo umount /home/matt/newsys

.PHONY: clean
clean:
	rm -rf build *.o
	cd tcp_pty; cargo clean

.PHONY: build_dir
build_dir:
	mkdir -p build/bin build/dev build/proc build/run \
		build/usr/bin build/usr/sbin build/sys

CC=musl-gcc

build/usr/sbin/tcp_pty: tcp_pty/src/main.rs tcp_pty/Cargo.toml
	cd tcp_pty; cargo build -r
	cp tcp_pty/target/x86_64-unknown-linux-musl/release/tcp_pty \
		build/usr/sbin/tcp_pty

build/usr/sbin/init: init/src/main.rs init/Cargo.toml
	cd init; cargo build -r
	cp init/target/x86_64-unknown-linux-musl/release/init \
		build/usr/sbin/init

build/usr/bin/login: login.c
	$(CC) -o build/usr/bin/login login.c

build/bin/sh: sh.c
	$(CC) -o build/bin/sh sh.c
