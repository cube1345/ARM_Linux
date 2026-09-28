#ifndef BRICK_GAME_H
#define BRICK_GAME_H

#include <QPointF>
#include <QVector>
#include <QWidget>

class QTimer;

class BrickGame : public QWidget
{
    Q_OBJECT
public:
    explicit BrickGame(QWidget *parent = nullptr);
    ~BrickGame() override;

    void startGame();
    void stopGame();
    void setPaddleDir(int dir);

signals:
    void gameOver(bool win, int score);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    static const int kBrickCols = 12;
    static const int kBrickRows = 8;
    static const int kBrickW = 60;
    static const int kBrickH = 18;
    static const int kPaddleW = 90;
    static const int kPaddleH = 16;

    QVector<QVector<bool> > m_bricks;
    float m_paddleX;
    int m_paddleDir;
    QPointF m_ball;
    QPointF m_vel;
    int m_score;
    bool m_alive;
    QTimer *m_timer;

    void resetGame();
    QRect brickRect(int row, int col) const;
    QRectF paddleRect() const;
};

#endif
