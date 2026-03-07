
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

clean:
	@if lsmod | grep -q "^$(MOD_NAME)"; then \
		echo sudo rmmod $(MOD_NAME); \
		sudo rmmod $(MOD_NAME); \
	fi
	rm -rf Module.symvers modules.order *.ko *.mod* *.o

test: module
	sudo insmod $(MOD_NAME).ko
	@if dmesg | grep -q "test_driver: module loaded"; then \
    echo "SUCCESS: driver loaded"; \
	else \
    	echo "FAIL: driver not loaded"; \
	fi
# ---------------------------
endif
