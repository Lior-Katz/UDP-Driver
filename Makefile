
ifneq ($(KERNELRELEASE),)
# --------- KBUILD ---------
obj-m := udp_write.o
# --------------------------

else
# ----- Normal Makefile -----
KDIR := /lib/modules/`uname -r`/build
MOD_NAME := udp_write

module:
	$(MAKE) -C $(KDIR) M=$$PWD

install: module
	sudo insmod $(MOD_NAME).ko

uninstall:
	@if lsmod | grep -q "^$(MOD_NAME)"; then \
		echo sudo rmmod $(MOD_NAME); \
		sudo rmmod $(MOD_NAME); \
	fi

clean: uninstall
	rm -rf Module.symvers modules.order *.ko *.mod* *.o .*.cmd
# ---------------------------
endif
