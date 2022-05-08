.PHONY: build
build: build_dir build/sbin/init build/bin/sh \
	build/bin/ls build/bin/mount build/bin/headc

build_dir:
	mkdir -p build
	mkdir -p build/bin
	mkdir -p build/sbin

build/sbin/init: build_dir init.c
	cc -o build/sbin/init init.c --static

build/bin/sh: build_dir sh.c
	cc -o build/bin/sh sh.c --static

build/bin/ls: build_dir ls.c
	cc -o build/bin/ls ls.c --static

build/bin/mount: build_dir mount.c
	cc -o build/bin/mount mount.c --static

build/bin/headc: build_dir headc.c
	cc -o build/bin/headc headc.c --static
