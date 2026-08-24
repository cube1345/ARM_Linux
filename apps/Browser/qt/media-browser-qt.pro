QT += widgets
CONFIG += c++11
TEMPLATE = app
TARGET = media-browser-qt
SOURCES += main.cpp main_window.cpp
SOURCES += ../media/audio/audio_player.c
SOURCES += ../media/video/media_player.c
SOURCES += ../media/video/video_decoder.c
SOURCES += ../media/image/image_data.c
SOURCES += ../core/browser_log.c
HEADERS += main_window.h

INCLUDEPATH += ../core ../media/audio ../media/video ../media/image
LIBS += -lasound -lmpg123 -lavformat -lavcodec -lavutil -lswscale -lswresample -lpthread

target.path = /usr/bin
INSTALLS += target
