#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <QList>
#include <QPoint>
#include <QWidget>

class QTimer;

class SnakeGame : public QWidget
{
    Q_OBJECT
public:
    explicit SnakeGame(QWidget *parent = nullptr);
    ~SnakeGame() override;

    enum Direction { DirUp = 0, DirDown, DirLeft, DirRight };
    void startGame();
    void stopGame();
    void setDirection(int dir);

signals:
    void gameOver(int score);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    static const int kCols = 20;
    static const int kRows = 14;
    static const int kCell = 40;

    QList<QPoint> m_snake;
    QPoint m_food;
    int m_dir;
    int m_nextDir;
    int m_score;
    bool m_alive;
    QTimer *m_timer;

    void resetGame();
    void spawnFood();
    QRect cellRect(int row, int col) const;
};

#endif
