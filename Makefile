.PHONY: build
build: build_dir build/sbin/init build/bin/sh \
	build/bin/ls build/bin/mount build/bin/headc

.PHONY: vmdk
vmdk: build
	sudo mount /dev/loop0p1 /home/matt/newsys; \
		sudo cp -R /home/matt/yip_linux/build/* /home/matt/newsys; \
		sudo umount /home/matt/newsys; \
		qemu-img convert -f raw -O vmdk /home/matt/disk.raw \
		/mnt/hgfs/matt/Documents/Virtual\ Machines/Yip/Yip.vmdk

.PHONY: clean
clean:
	rm -rf build

.PHONY: build_dir
build_dir:
	mkdir -p build/bin build/dev build/proc build/run \
		build/sbin build/sys

CC=musl-gcc

build/sbin/init: init.c
	$(CC) -o build/sbin/init init.c --static

build/bin/sh: sh.c
	$(CC) -o build/bin/sh sh.c --static

build/bin/ls: ls.c
	$(CC) -o build/bin/ls ls.c --static

build/bin/mount: mount.c
	$(CC) -o build/bin/mount mount.c --static

build/bin/headc: headc.c
	$(CC) -o build/bin/headc headc.c --static
