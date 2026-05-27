CXX:=/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-g++
CXXFLAGS:=-O2 -march=armv5te -mtune=arm926ej-s -marm -msoft-float -funsigned-char
SDL_CONFIG:=$(shell $(CXX) -print-sysroot)/usr/bin/sdl-config
