QT += widgets concurrent network
CONFIG += c++11
TEMPLATE = app
TARGET = photo-album
SOURCES += main.cpp main_window.cpp photo_view.cpp video_player.cpp
HEADERS += main_window.h photo_view.h video_player.h
LIBS += -lavformat -lavcodec -lavutil -lswscale
