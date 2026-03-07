
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

install: $(MOD_NAME).ko
	sudo insmod $(MOD_NAME).ko

clean:
	@if lsmod | grep -q "^$(MOD_NAME)"; then \
		echo sudo rmmod $(MOD_NAME); \
		sudo rmmod $(MOD_NAME); \
	fi
	rm -rf Module.symvers modules.order *.ko *.mod* *.o

test: module
	sudo dmesg -C
	$(MAKE) install

	$(call CHECK, grep -q "$(DRIVER_NAME)" /proc/devices, \
		device major allocated, \
		device major not allocated)

	$(call CHECK, dmesg | grep -q "$(MOD_NAME): registered device", \
		registered device, \
		failed registering device)

	$(call CHECK, dmesg | grep -q "$(MOD_NAME): node created", \
		driver loaded, \
		driver not loaded)

	$(call CHECK, dmesg | grep -q "$(MOD_NAME): module loaded", \
		driver loaded, \
		driver not loaded)

# Check sysfs class
	ls /sys/class/${DRIVER_NAME}
	$(call CHECK, test -d /sys/class/${DRIVER_NAME}, \
		class $(DRIVER_NAME) registered, \
		class $(DRIVER_NAME) not registered)

# Check sysfs device
	$(call CHECK, test -d /sys/class/${DRIVER_NAME}/${DRIVER_NAME}, \
		device ${DRIVER_NAME} registered in sysfs, \
		device ${DRIVER_NAME} not registered in sysfs)

# Check dev node created by udev
	$(call CHECK, test -e /dev/${DRIVER_NAME}, \
		dev node created, \
		dev node not created)

	$(call CHECK, \
		stat -c "%t" /dev/$(DRIVER_NAME) | grep -qi $$(printf "%x" $$(awk '$$2=="$(DRIVER_NAME)" {print $$1}' /proc/devices)), \
		dev node major correct, \
		dev node major mismatch)

# Write to device to trigger .write
	@echo "test" | sudo tee /dev/${DRIVER_NAME} > /dev/null


# Check that write was called
	$(call CHECK, dmesg | grep -q "$(MOD_NAME): write called", \
		write invoked, \
		write not invoked)
	
	$(MAKE) clean

	$(call CHECK, ! lsmod | grep -q "^$(MOD_NAME)", \
		module removed, \
		module still loaded)

	$(call CHECK, ! grep -q "$(DRIVER_NAME)" /proc/devices, \
		device major unregistered, \
		device major still registered)

# Ensure sysfs class removed
	$(call CHECK, ! test -d /sys/class/${DRIVER_NAME}, \
		class $(DRIVER_NAME) removed, \
		class $(DRIVER_NAME) still exists)

# Ensure sysfs device removed
	$(call CHECK, ! test -d /sys/class/${DRIVER_NAME}/${DRIVER_NAME}, \
		device ${DRIVER_NAME} removed from sysfs, \
		device ${DRIVER_NAME} still exists in sysfs)

# Ensure dev node removed
	$(call CHECK, ! test -e /dev/${DRIVER_NAME}, \
		dev node removed, \
		dev node still exists)
# ---------------------------
endif
