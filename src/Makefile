obj-m := iomemory-vsl4.o

EXTRA_CFLAGS += -I$(PWD)/include

KDIR := /lib/modules/$(KERNELRELEASE)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
