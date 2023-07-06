obj-m += blaster.o
CFLAGS_blaster.o := -Wno-declaration-after-statement

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test:
	sudo dmesg -C
	sudo insmod blaster.ko
	sudo sh major_num_extr.sh
	sudo mknod -m 666 /dev/blaster c "$(shell cat 'major_num.txt')" 0
	sudo dmesg -wH

untest:
	sudo rmmod blaster
	sudo rm -f /dev/blaster
