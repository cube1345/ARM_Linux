#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <QWidget>

#include "tetris_logic.h"

class QTimer;

class TetrisGame : public QWidget
{
    Q_OBJECT
public:
    explicit TetrisGame(QWidget *parent = nullptr);
    ~TetrisGame() override;

    void startGame();
    void stopGame();
    void moveLeft();
    void moveRight();
    void rotate();
    void softDrop();
    void hardDrop();

signals:
    void gameOver(int score);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    static const int kCell = 26;

    TetrisLogic m_logic;
    QTimer *m_timer;

    QRect cellRect(int row, int col) const;
    QColor colorOf(int idx) const;
};

#endif
