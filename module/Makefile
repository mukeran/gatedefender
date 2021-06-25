DEV_NAME = gatedefender
# target object name
obj-m += $(DEV_NAME).o
# using find to list all sources files
src-s = $(shell find $(PWD)  -type f -name '*.c' -exec realpath --relative-to $(PWD) {} \;)
# convert source list to object list
$(DEV_NAME)-objs := $(patsubst %.c, %.o, $(src-s))
BUILD_DIR = ../dist
KERNEL_VERSION=3.10.0
# include debug info
KERNEL_PATH = /root/linux-3.10.0-1160.el7
MY_CFLAGS += -g -DDEBUG -O0 -I$(KERNEL_PATH)
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	make release
release:
	make -C $(KERNEL_PATH) M=$(PWD) modules
	make move
debug:
	EXTRA_CFLAGS="$(MY_CFLAGS)"
	make -C $(KERNEL_PATH) M=$(PWD) modules
	make move
move:
	mv .tmp_versions $(BUILD_DIR)
	mv *.symvers $(BUILD_DIR)
	mv *.order $(BUILD_DIR)
	find . -name ".*.cmd" | xargs -I {} mv {} $(BUILD_DIR)
	mv *.ko $(BUILD_DIR)
	find . -name "*.o" | xargs -I {} mv {} $(BUILD_DIR)
	mv *.mod.c $(BUILD_DIR)
clean:
	rm -rf $(BUILD_DIR)
	mkdir $(BUILD_DIR)
