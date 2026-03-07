
ifneq ($(KERNELRELEASE),)
# --------- KBUILD ---------
obj-m := udp_write.o
# --------------------------

else
# ----- Normal Makefile -----
KDIR := /lib/modules/`uname -r`/build
MOD_NAME := udp_write
DRIVER_NAME := udp

define CHECK
@if $(1); then \
	echo "SUCCESS: $(2)"; \
else \
	echo "FAIL: $(3)"; \
fi
endef

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
	$(call CHECK, dmesg | grep -q "$(MOD_NAME): module loaded", \
		driver loaded, \
		driver not loaded)

	$(call CHECK, grep -q "$(DRIVER_NAME)" /proc/devices, \
		device major allocated, \
		device major not allocated)
	
	$(MAKE) clean

	$(call CHECK, ! lsmod | grep -q "^$(MOD_NAME)", \
		module removed, \
		module still loaded)

	$(call CHECK, ! grep -q "$(DRIVER_NAME)" /proc/devices, \
		device major unregistered, \
		device major still registered)
# ---------------------------
endif
