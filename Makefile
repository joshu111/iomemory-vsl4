KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)/src

obj-m += iomemory-vsl4.o

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
