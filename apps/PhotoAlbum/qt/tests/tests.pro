QT += testlib
QT -= gui
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tests

INCLUDEPATH += ..

SOURCES += \
    tst_games.cpp \
    ../game2048_logic.cpp \
    ../tetris_logic.cpp

HEADERS += \
    ../game2048_logic.h \
    ../tetris_logic.h
