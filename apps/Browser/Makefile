CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc

TARGET := image_browser
PKG_CONFIG ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/pkg-config
PACKAGES := freetype2 alsa libjpeg libpng libmpg123
SOURCES := image_browser.c bmp_display.c video_buffer.c input_keyboard.c \
	   image_data.c bmp_decoder.c jpeg_decoder.c png_decoder.c \
	   gif_animation.c animation_decoder.c image_decoder.c \
	   image_render.c file_list.c font_renderer.c ui_draw.c browser_ui.c \
	   page_file.c page_image.c page_text.c page_audio.c text_reader.c \
	   audio_player.c
OBJECTS := $(SOURCES:.c=.o)
CFLAGS ?= -Wall -Wextra -Wpedantic -O2
CPPFLAGS := $(shell $(PKG_CONFIG) --cflags-only-I $(PACKAGES))
LDLIBS := $(shell $(PKG_CONFIG) --libs $(PACKAGES)) -lgif -pthread

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -D -m 0755 $(TARGET) $(DESTDIR)/usr/bin/media-browser

clean:
	rm -f $(TARGET) $(OBJECTS)
