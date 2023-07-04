obj-m += blaster.o
flagz = "-Wall -Wextra -pedantic -std=c17"
KCFLAGS += "$(flagz)"
USERCFLAGS += "$(flagz)"

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test:
	sudo dmesg -C
	sudo insmod blaster.ko
	sudo mknod -m 666 /dev/blaster c 239 0
	#sudo mknod -m 666 /dev/blaster c "$(shell sh ./major_num_extr.sh && cat major_num.txt)" 0
	sudo dmesg -wH

untest:
	sudo rmmod blaster
	sudo rm -f /dev/blaster
