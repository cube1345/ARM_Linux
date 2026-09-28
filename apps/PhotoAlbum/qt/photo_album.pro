QT += widgets concurrent network
CONFIG += c++11
TEMPLATE = app
TARGET = photo-album
SOURCES += main.cpp main_window.cpp photo_view.cpp video_player.cpp icm20608.cpp tank_game.cpp snake_game.cpp tetris_game.cpp tetris_logic.cpp brick_game.cpp game2048.cpp game2048_logic.cpp music_player.cpp weather.cpp calculator.cpp drawboard.cpp ap3216c.cpp icon_factory.cpp debug_page.cpp debug_monitor.cpp debug_overlay.cpp
HEADERS += main_window.h photo_view.h video_player.h icm20608.h tank_game.h snake_game.h tetris_game.h tetris_logic.h brick_game.h game2048.h game2048_logic.h music_player.h weather.h calculator.h drawboard.h ap3216c.h icon_factory.h debug_page.h debug_monitor.h debug_overlay.h
LIBS += -lavformat -lavcodec -lavutil -lswscale -lasound
