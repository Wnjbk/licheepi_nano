#!/bin/bash
# 设置 Buildroot 工具链路径
BR_HOST_DIR="$HOME/LicheePi_Nano/buildroot-2018.02.11/output/host"
SYSROOT="$BR_HOST_DIR/arm-buildroot-linux-uclibcgnueabi/sysroot"

export PATH="$BR_HOST_DIR/bin:$PATH"

CC=arm-buildroot-linux-uclibcgnueabi-gcc
CXX=arm-buildroot-linux-uclibcgnueabi-g++
STRIP=arm-buildroot-linux-uclibcgnueabi-strip

CFLAGS="--sysroot=$SYSROOT -O3 -marm -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft -Wall -I$SYSROOT/usr/include"
LDFLAGS="--sysroot=$SYSROOT -L$SYSROOT/usr/lib -lSDL -lSDL_mixer -lpthread -lm -lz"

# 开始编译（直接 make 会调用 sdl/Makefile 内的规则）
make -j4 \
    CC="$CC" \
    CXX="$CXX" \
    STRIP="$STRIP" \
    CFLAGS="$CFLAGS" \
    LDFLAGS="$LDFLAGS" \
    all
