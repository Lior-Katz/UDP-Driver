
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

	$(call CHECK, dmesg | grep -q "$(MOD_NAME): module loaded", \
		driver loaded, \
		driver not loaded)

# Create temporary device node
	MAJOR=$$(awk '$2=="$(DRIVER_NAME)" {print $1}' /proc/devices) \
	echo "Using major $$MAJOR"; \
	sudo mknod /tmp/udp c $$MAJOR 0; \
	sudo chmod 666 /tmp/udp

# Write to device to trigger .write
	@echo "test" > /tmp/udp

# Check that write was called
	$(call CHECK, dmesg | grep -q "$(MOD_NAME): write called", write invoked, write not invoked)

# Cleanup device node
	sudo rm -f /tmp/udp
	
	$(MAKE) clean

	$(call CHECK, ! lsmod | grep -q "^$(MOD_NAME)", \
		module removed, \
		module still loaded)

	$(call CHECK, ! grep -q "$(DRIVER_NAME)" /proc/devices, \
		device major unregistered, \
		device major still registered)
# ---------------------------
endif
