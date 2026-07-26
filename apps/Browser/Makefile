CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc

TARGET := image_browser
SOURCES := image_browser.c bmp_display.c input_keyboard.c image_list.c
OBJECTS := $(SOURCES:.c=.o)
CFLAGS := -Wall -Wextra -Wpedantic -O2

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS)
