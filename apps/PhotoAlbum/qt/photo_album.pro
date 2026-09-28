QT += widgets concurrent network
CONFIG += c++11
TEMPLATE = app
TARGET = photo-album
SOURCES += main.cpp main_window.cpp photo_view.cpp video_player.cpp icm20608.cpp tank_game.cpp snake_game.cpp tetris_game.cpp brick_game.cpp game2048.cpp music_player.cpp weather.cpp calculator.cpp drawboard.cpp ap3216c.cpp
HEADERS += main_window.h photo_view.h video_player.h icm20608.h tank_game.h snake_game.h tetris_game.h brick_game.h game2048.h music_player.h weather.h calculator.h drawboard.h ap3216c.h
LIBS += -lavformat -lavcodec -lavutil -lswscale -lasound
