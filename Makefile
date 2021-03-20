obj-m := wiiucon_rpi.o
KVERSION := 5.10.17-v7l+
ccflags-y := -I/usr/include

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean