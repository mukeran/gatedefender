ROOT=$(PWD)
TEST_SOURCES=$(wildcard test/*.c)
TEST_OBJ=$(TEST_SOURCES:.c=.o)
TEST_TARGETS=$(TEST_SOURCES:.c=.test)
INCLUDE=lib/include
.PHONY: module clean run debug setup_network remove_network
all: module
module: clean
	cd module && $(MAKE) $(DEBUG)
clean:
	rm -rf dist
	mkdir dist
	rm -rf emulator/initfs/data
run: module
	cp -r dist emulator/initfs/data
	cd emulator/initfs && find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../rootfs
	cd $(ROOT) && qemu-system-x86_64 \
		-nographic \
		-kernel emulator/bzImage \
		-initrd emulator/rootfs \
		-append "root=/dev/ram nokaslr console=ttyS0 rdinit=/init" \
		-curses \
		-m 512m \
		-serial stdio \
		-netdev tap,id=devnet0,ifname=tap0,script=no,downscript=no \
		-device e1000,netdev=devnet0,mac=26:E1:20:D1:44:46 \
		$(EXTRA)
debug:
	EXTRA="-s -S" DEBUG='debug' $(MAKE) run
setup_network:
	tunctl -t tap0 -u root
	ip link set tap0 up
	ip addr add 172.16.222.1/24 dev tap0
remove_network:
	ip link set tap0 down
	tunctl -d tap0
