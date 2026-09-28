#ifndef TETRIS_LOGIC_H
#define TETRIS_LOGIC_H

#include <QVector>

/**
 * @brief 俄罗斯方块纯逻辑（无 UI 依赖，可单元测试）。
 */
class TetrisLogic
{
public:
    static const int kCols = 10;
    static const int kRows = 20;

    TetrisLogic();

    void reset();

    bool collides(int type, int rot, int x, int y) const;
    bool moveLeft();
    bool moveRight();
    bool rotate();
    bool softDrop();
    void hardDrop();
    /** @brief 计时下落一格；到底则锁定并生成新块。返回本次消除的行数。 */
    int tick();

    int type() const { return m_type; }
    int rot() const { return m_rot; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int score() const { return m_score; }
    bool alive() const { return m_alive; }
    int cell(int r, int c) const;

    /** @brief 方块第 i 格相对锚点的偏移。 */
    static int shapeDx(int type, int rot, int i);
    static int shapeDy(int type, int rot, int i);

    // 测试用
    void setCell(int r, int c, int value);
    void setPiece(int type, int rot, int x, int y);
    int lockPiece();   // 锁定当前方块并消行，返回消除行数
    int clearLines();  // 只消行，返回消除行数

private:
    QVector<QVector<int> > m_board;
    int m_type;
    int m_rot;
    int m_x;
    int m_y;
    int m_score;
    bool m_alive;

    static const int kShapes[7][4][4][2];

    void newPiece();
};

#endif
