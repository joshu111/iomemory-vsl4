obj-m := iomemory-vsl4.o

EXTRA_CFLAGS += -I$(PWD)/include -I$(PWD)/include/fio -I$(PWD)/include/fio/port -I$(PWD)/include/fio/port/linux

KDIR := /lib/modules/$(KERNELRELEASE)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
