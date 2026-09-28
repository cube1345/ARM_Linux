#ifndef GAME2048_LOGIC_H
#define GAME2048_LOGIC_H

/**
 * @brief 2048 游戏纯逻辑（无 UI 依赖，可单元测试）。
 */
class Game2048Logic
{
public:
    enum Direction { DirUp = 0, DirDown, DirLeft, DirRight };
    static const int kSize = 4;

    Game2048Logic();

    /** @brief 清空棋盘并随机生成两个初始块。 */
    void reset();
    /** @brief 移动并合并；发生移动时随机生成新块。返回是否有移动。 */
    bool move(int dir);
    /** @brief 只移动合并、不生成新块（测试用）。 */
    bool moveNoSpawn(int dir);

    int cell(int r, int c) const;
    void setCell(int r, int c, int value);
    int score() const { return m_score; }
    bool canMove() const;

private:
    int m_board[kSize][kSize];
    int m_score;

    void spawnRandom();
    bool slideLine(int line[kSize]);
};

#endif
