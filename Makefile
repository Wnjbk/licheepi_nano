LVGL_DIR := third_party/lvgl
BUILD_DIR := build

CC := gcc
CXX := g++
CFLAGS := -O2 -Wall -Wextra -I. -I$(LVGL_DIR) -I$(LVGL_DIR)/src \
	-DLV_CONF_INCLUDE_SIMPLE $(shell sdl-config --cflags)
CXXFLAGS := $(CFLAGS) -std=c++11
LDLIBS := $(shell sdl-config --libs) -lm -lpthread

LVGL_C_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJS := $(patsubst $(LVGL_DIR)/%.c,$(BUILD_DIR)/lvgl/%.o,$(LVGL_C_SRCS))
APP_OBJ := $(BUILD_DIR)/lvgl_host_poc.o

all: $(BUILD_DIR)/wiliwili-lvgl-host-poc

$(BUILD_DIR)/wiliwili-lvgl-host-poc: $(LVGL_OBJS) $(APP_OBJ)
	$(CXX) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/lvgl/%.o: $(LVGL_DIR)/%.c lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(APP_OBJ): src/lvgl_host_poc.cpp lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
