BUILDROOT_OUTPUT ?= /home/cube/WorkSpace/Linux/ARM_Linux
TARGET_TRIPLE ?= aarch64-buildroot-linux-gnu
CROSS_COMPILE ?= $(BUILDROOT_OUTPUT)/host/bin/$(TARGET_TRIPLE)-
CC := $(CROSS_COMPILE)gcc

TARGET := image_browser
HOST_CC ?= cc
TEST_MEDIA_DIR ?= $(BUILDROOT_OUTPUT)/target/root/media
PKG_CONFIG ?= $(BUILDROOT_OUTPUT)/host/bin/pkg-config
PACKAGES := freetype2 alsa libjpeg libpng libmpg123 libavformat libavcodec \
	    libavutil libswscale libswresample
INCLUDE_DIRS := app core platform/display platform/input platform/font \
	       media/image media/animation media/audio media/video \
	       pages/desktop pages/files pages/gallery pages/reader pages/player \
	       pages/diagnostics pages/tools pages/settings ui
SOURCES := app/main.c app/browser_app.c \
	   core/browser_log.c core/debug_manager.c core/desktop_app.c \
	   core/file_list.c core/page_manager.c \
	   platform/display/bmp_display.c platform/display/video_buffer.c \
	   platform/display/display_manager.c \
	   platform/input/input_keyboard.c \
	   platform/font/font_renderer.c platform/font/font_manager.c \
	   media/image/image_data.c media/image/bmp_decoder.c \
	   media/image/jpeg_decoder.c media/image/png_decoder.c \
	   media/image/image_decoder.c media/image/image_render.c \
	   media/animation/gif_animation.c media/animation/animation_decoder.c \
	   media/audio/audio_player.c media/video/media_player.c \
	   pages/desktop/page_desktop.c pages/files/page_file.c \
	   pages/gallery/page_gallery.c pages/gallery/page_image.c \
	   pages/reader/page_text.c \
	   pages/reader/text_reader.c pages/player/page_audio.c \
	   pages/player/page_video.c pages/diagnostics/page_diagnostics.c \
	   pages/tools/page_tools.c pages/settings/page_settings.c \
	   ui/ui_draw.c ui/browser_ui.c
OBJECTS := $(SOURCES:.c=.o)
CFLAGS ?= -Wall -Wextra -Wpedantic -O2
CPPFLAGS := $(shell $(PKG_CONFIG) --cflags-only-I $(PACKAGES)) \
	       $(addprefix -I,$(INCLUDE_DIRS))
LDLIBS := $(shell $(PKG_CONFIG) --libs $(PACKAGES)) -lgif -pthread

.PHONY: all clean install test test-media test-input

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -D -m 0755 $(TARGET) $(DESTDIR)/usr/bin/media-browser

test: test-media

test-media:
	CC="$(HOST_CC)" ./tests/media_smoke.sh "$(TEST_MEDIA_DIR)"

test-input:
	./tests/input_auto_smoke.sh

clean:
	rm -f $(TARGET) $(OBJECTS)
