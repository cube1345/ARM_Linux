#ifndef GAME2048_H
#define GAME2048_H

#include <QWidget>

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
    static const int kSize = 4;
    int m_board[kSize][kSize];
    int m_score;
    bool m_alive;

    void resetGame();
    void spawnRandom();
    bool slideLine(int line[4]);
    bool canMove() const;
    QColor colorOf(int val) const;
};

#endif
