QT += widgets
CONFIG += c++11
TEMPLATE = app
TARGET = media-browser-qt
SOURCES += main.cpp main_window.cpp
HEADERS += main_window.h

target.path = /usr/bin
INSTALLS += target
