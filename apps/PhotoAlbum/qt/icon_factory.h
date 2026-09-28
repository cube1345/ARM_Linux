#ifndef ICON_FACTORY_H
#define ICON_FACTORY_H

#include <QPixmap>

class IconFactory
{
public:
    static QPixmap album();
    static QPixmap monitor();
    static QPixmap video();
    static QPixmap game();
    static QPixmap snake();
    static QPixmap tetris();
    static QPixmap brick();
    static QPixmap game2048();
    static QPixmap music();
    static QPixmap calculator();
    static QPixmap draw();
    static QPixmap settings();
    static QPixmap debug();
};

#endif
