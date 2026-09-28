#ifndef GAME2048_H
#define GAME2048_H

#include <QWidget>

#include "game2048_logic.h"

class Game2048 : public QWidget
{
    Q_OBJECT
public:
    explicit Game2048(QWidget *parent = nullptr);

    enum Direction { DirUp = 0, DirDown, DirLeft, DirRight };
    void startGame();
    void move(int dir);

signals:
    void gameOver(bool win);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Game2048Logic m_logic;
    bool m_alive;

    QColor colorOf(int val) const;
};

#endif
