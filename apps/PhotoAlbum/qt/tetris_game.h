#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <QPoint>
#include <QVector>
#include <QWidget>

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
    static const int kCols = 10;
    static const int kRows = 20;
    static const int kCell = 26;

    QVector<QVector<int> > m_board;  // 0=空, 1~7=方块颜色索引
    int m_type;
    int m_rot;
    int m_x;
    int m_y;
    int m_score;
    bool m_alive;
    QTimer *m_timer;

    static const int kShapes[7][4][4][2];

    void resetGame();
    void newPiece();
    void lockPiece();
    void clearLines();
    bool collides(int type, int rot, int x, int y) const;
    QRect cellRect(int row, int col) const;
    QColor colorOf(int idx) const;
};

#endif
