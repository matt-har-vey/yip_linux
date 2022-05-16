.PHONY: build
build: build_dir build/usr/sbin/init build/bin/sh \
	build/bin/mount build/usr/sbin/shelld

.PHONY: rawdisk
rawdisk: build
	sudo mount /dev/loop0p1 /home/matt/newsys; \
		sudo cp -R /home/matt/yip_linux/build/* /home/matt/newsys; \
		sudo umount /home/matt/newsys

.PHONY: vmdk
vmdk: rawdisk
		qemu-img convert -f raw -O vmdk /home/matt/disk.raw \
		/mnt/hgfs/matt/Documents/Virtual\ Machines/Yip/Yip.vmdk

.PHONY: clean
clean:
	rm -rf build
	rm *.o

.PHONY: build_dir
build_dir:
	mkdir -p build/bin build/dev build/proc build/run \
		build/usr/sbin build/sys

CC=musl-gcc

exec_shell.o: exec_shell.h exec_shell.c
	$(CC) -o exec_shell.o -c exec_shell.c

build/usr/sbin/init: init.c exec_shell.o
	$(CC) -o build/usr/sbin/init init.c exec_shell.o --static

build/usr/sbin/shelld: shelld.c exec_shell.o
	$(CC) -o build/usr/sbin/shelld shelld.c exec_shell.o --static

build/bin/sh: sh.c
	$(CC) -o build/bin/sh sh.c --static

build/bin/mount: mount.c
	$(CC) -o build/bin/mount mount.c --static
