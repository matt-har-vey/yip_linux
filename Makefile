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

.PHONY: build
build: build_dir build/sbin/init build/bin/sh build/bin/ls
