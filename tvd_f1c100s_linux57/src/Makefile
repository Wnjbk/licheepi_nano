MODNAME = suniv_f1c100s_tvd

KBUILD ?= ~/LicheePi_Nano/linux/
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabi- 

SRC_DIR := $(shell pwd)

ifeq (,$(KBUILD))
$(error Kernel build tree not found - please set KBUILD to configured kernel)
endif

KCONFIG := $(KBUILD)/.config
ifeq (,$(wildcard $(KCONFIG)))
$(error No .config found in $(KBUILD), please set KBUILD to configured kernel)
endif

VERSION_H := $(KBUILD)/include/generated/utsrelease.h
ifeq (,$(wildcard $(VERSION_H)))
VERSION_H := $(KBUILD)/include/linux/utsrelease.h
endif
ifeq (,$(wildcard $(VERSION_H)))
VERSION_H := $(KBUILD)/include/linux/version.h
endif
ifeq (,$(wildcard $(VERSION_H)))
$(error Please run 'make modules_prepare' in $(KBUILD))
endif

obj-m := $(MODNAME).o

all: modules

modules:
	$(MAKE) -C $(KBUILD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(SRC_DIR) modules

clean:
	$(MAKE) -C $(KBUILD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(SRC_DIR) clean

.PHONY: all modules clean
