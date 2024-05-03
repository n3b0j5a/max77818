obj-m += max77818.o
obj-m += max77818_charger.o
obj-m += max77818_battery.o
obj-m += max77818-regulator.o

KERNEL_DIR ?= /usr/src/linux
CROSS_COMPILE ?= arm-linux-gnueabihf-

all:
	make -C $(KERNEL_DIR) \
ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) \
			M=$(shell pwd) modules

clean:
	make -C $(KERNEL_DIR) \
				ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) \
							M=$(shell pwd) clean

install:
	cp max77818.c $(KERNEL_DIR)/drivers/mfd
	cp max77818_charger.c $(KERNEL_DIR)/drivers/power/supply
	cp max77818_battery.c $(KERNEL_DIR)/drivers/power/supply
	cp max77818-regulator.c $(KERNEL_DIR)/drivers/regulator
	cp max77818.h $(KERNEL_DIR)/include/linux/mfd
	cp max77818_charger.h $(KERNEL_DIR)/include/linux/power
	cp max77818_battery.h $(KERNEL_DIR)/include/linux/power
	cp max77818-private.h $(KERNEL_DIR)/include/linux/mfd
