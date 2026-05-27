CROSS_COMPILE ?= ~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-linux-
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -O2 -Wall -Wextra

all: tvd_fb_preview

tvd_fb_preview: tvd_fb_preview.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f tvd_fb_preview

.PHONY: all clean
