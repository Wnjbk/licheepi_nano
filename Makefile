LVGL_DIR := /home/wnk/F1C200S_host_archive/wiliwili_lite_lvgl_host_poc_20260905/third_party/lvgl
KERNEL_DIR := /home/wnk/F1C200S_host_archive/linux_sii9022_lvgl_de_osd_20260905
BUILD_DIR := build-arm
TOOLCHAIN := /opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-

CC := $(TOOLCHAIN)gcc
CXX := $(TOOLCHAIN)g++
CFLAGS := -O2 -Wall -Wextra -I. -I$(LVGL_DIR) -I$(LVGL_DIR)/src \
	-I$(KERNEL_DIR)/include/uapi -DLV_CONF_INCLUDE_SIMPLE
CXXFLAGS := $(CFLAGS) -std=c++11
LDLIBS := -lm -lpthread

LVGL_C_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJS := $(patsubst $(LVGL_DIR)/%.c,$(BUILD_DIR)/lvgl/%.o,$(LVGL_C_SRCS))
APP_OBJ := $(BUILD_DIR)/lvgl_de_osd.o

all: $(BUILD_DIR)/lvgl-de-osd

$(BUILD_DIR)/lvgl-de-osd: $(LVGL_OBJS) $(APP_OBJ)
	$(CXX) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/lvgl/%.o: $(LVGL_DIR)/%.c lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(APP_OBJ): lvgl_de_osd.cpp lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
