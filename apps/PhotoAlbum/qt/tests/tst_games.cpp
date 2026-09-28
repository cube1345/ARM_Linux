#include <QtTest>

#include "game2048_logic.h"
#include "tetris_logic.h"

/**
 * @brief 游戏纯逻辑单元测试（主机运行，不依赖板卡硬件）。
 */
class TestGames : public QObject
{
    Q_OBJECT

private slots:
    // ---------- 2048 ----------
    void test2048InitialHasTwoTiles();
    void test2048PlainSlideLeft();
    void test2048MergeLeft();
    void test2048MergeFourTiles();
    void test2048NoMergeDifferent();
    void test2048MoveRight();
    void test2048MoveUp();
    void test2048ScoreAccumulates();
    void test2048CannotMoveWhenFullAndDistinct();
    void test2048CanMoveWhenEmptyExists();

    // ---------- 俄罗斯方块 ----------
    void testTetrisBoardEmptyInitially();
    void testTetrisCollidesFloor();
    void testTetrisCollidesLockedBlock();
    void testTetrisClearOneLine();
    void testTetrisClearTwoLines();
    void testTetrisLockPiece();
    void testTetrisMoveLeftBlockedByWall();
    void testTetrisHardDropLands();

private:
    static void clearBoard(Game2048Logic &logic);
};

void TestGames::clearBoard(Game2048Logic &logic)
{
    for (int r = 0; r < Game2048Logic::kSize; ++r)
        for (int c = 0; c < Game2048Logic::kSize; ++c)
            logic.setCell(r, c, 0);
}

void TestGames::test2048InitialHasTwoTiles()
{
    Game2048Logic logic;
    int tiles = 0;
    for (int r = 0; r < Game2048Logic::kSize; ++r)
        for (int c = 0; c < Game2048Logic::kSize; ++c)
            if (logic.cell(r, c) != 0)
                ++tiles;
    QCOMPARE(tiles, 2);
}

void TestGames::test2048PlainSlideLeft()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 1, 2);
    logic.setCell(0, 3, 4);
    const bool moved = logic.moveNoSpawn(Game2048Logic::DirLeft);
    QVERIFY(moved);
    QCOMPARE(logic.cell(0, 0), 2);
    QCOMPARE(logic.cell(0, 1), 4);
    QCOMPARE(logic.cell(0, 2), 0);
    QCOMPARE(logic.cell(0, 3), 0);
}

void TestGames::test2048MergeLeft()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 0, 2);
    logic.setCell(0, 1, 2);
    logic.moveNoSpawn(Game2048Logic::DirLeft);
    QCOMPARE(logic.cell(0, 0), 4);
    QCOMPARE(logic.cell(0, 1), 0);
    QCOMPARE(logic.score(), 4);
}

void TestGames::test2048MergeFourTiles()
{
    Game2048Logic logic;
    clearBoard(logic);
    for (int c = 0; c < Game2048Logic::kSize; ++c)
        logic.setCell(0, c, 4);
    logic.moveNoSpawn(Game2048Logic::DirLeft);
    QCOMPARE(logic.cell(0, 0), 8);
    QCOMPARE(logic.cell(0, 1), 8);
    QCOMPARE(logic.cell(0, 2), 0);
    QCOMPARE(logic.cell(0, 3), 0);
    QCOMPARE(logic.score(), 16);
}

void TestGames::test2048NoMergeDifferent()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 0, 2);
    logic.setCell(0, 1, 4);
    logic.moveNoSpawn(Game2048Logic::DirLeft);
    QCOMPARE(logic.cell(0, 0), 2);
    QCOMPARE(logic.cell(0, 1), 4);
    QCOMPARE(logic.score(), 0);
}

void TestGames::test2048MoveRight()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 0, 2);
    logic.setCell(0, 1, 2);
    logic.moveNoSpawn(Game2048Logic::DirRight);
    QCOMPARE(logic.cell(0, Game2048Logic::kSize - 1), 4);
    QCOMPARE(logic.cell(0, Game2048Logic::kSize - 2), 0);
}

void TestGames::test2048MoveUp()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 0, 2);
    logic.setCell(1, 0, 2);
    logic.moveNoSpawn(Game2048Logic::DirUp);
    QCOMPARE(logic.cell(0, 0), 4);
    QCOMPARE(logic.cell(1, 0), 0);
}

void TestGames::test2048ScoreAccumulates()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 0, 2);
    logic.setCell(0, 1, 2);
    logic.moveNoSpawn(Game2048Logic::DirLeft);
    QCOMPARE(logic.score(), 4);
    logic.setCell(1, 0, 8);
    logic.setCell(1, 1, 8);
    logic.moveNoSpawn(Game2048Logic::DirLeft);
    QCOMPARE(logic.score(), 4 + 16);
}

void TestGames::test2048CannotMoveWhenFullAndDistinct()
{
    Game2048Logic logic;
    int v = 2;
    for (int r = 0; r < Game2048Logic::kSize; ++r)
        for (int c = 0; c < Game2048Logic::kSize; ++c) {
            logic.setCell(r, c, v);
            v *= 2;
        }
    QVERIFY(!logic.canMove());
}

void TestGames::test2048CanMoveWhenEmptyExists()
{
    Game2048Logic logic;
    clearBoard(logic);
    logic.setCell(0, 0, 2);
    QVERIFY(logic.canMove());
}

void TestGames::testTetrisBoardEmptyInitially()
{
    TetrisLogic logic;
    int filled = 0;
    for (int r = 0; r < TetrisLogic::kRows; ++r)
        for (int c = 0; c < TetrisLogic::kCols; ++c)
            if (logic.cell(r, c) != 0)
                ++filled;
    QCOMPARE(filled, 0);
}

void TestGames::testTetrisCollidesFloor()
{
    TetrisLogic logic;
    // I 方块 rot0 的 dy 含 1，放到最后一行会越界
    QVERIFY(logic.collides(0, 0, 0, TetrisLogic::kRows - 1));
    QVERIFY(!logic.collides(0, 0, 0, TetrisLogic::kRows - 2));
}

void TestGames::testTetrisCollidesLockedBlock()
{
    TetrisLogic logic;
    logic.setCell(5, 3, 1);
    // I rot0: 格子为 (x+0..3, y+1)。令 y+1=5 → y=4, x+0=3 → x=3
    QVERIFY(logic.collides(0, 0, 3, 4));
    QVERIFY(!logic.collides(0, 0, 4, 4));
}

void TestGames::testTetrisClearOneLine()
{
    TetrisLogic logic;
    for (int c = 0; c < TetrisLogic::kCols; ++c)
        logic.setCell(TetrisLogic::kRows - 1, c, 1);
    QCOMPARE(logic.clearLines(), 1);
    QCOMPARE(logic.score(), 100);
    for (int c = 0; c < TetrisLogic::kCols; ++c)
        QCOMPARE(logic.cell(TetrisLogic::kRows - 1, c), 0);
}

void TestGames::testTetrisClearTwoLines()
{
    TetrisLogic logic;
    for (int c = 0; c < TetrisLogic::kCols; ++c) {
        logic.setCell(TetrisLogic::kRows - 1, c, 1);
        logic.setCell(TetrisLogic::kRows - 2, c, 1);
    }
    QCOMPARE(logic.clearLines(), 2);
    QCOMPARE(logic.score(), 200);
}

void TestGames::testTetrisLockPiece()
{
    TetrisLogic logic;
    // O 方块 (type=1) rot0: 格子 (1,0),(2,0),(1,1),(2,1) 相对锚点
    logic.setPiece(1, 0, 0, 0);
    logic.lockPiece();
    QCOMPARE(logic.cell(0, 1), 2);
    QCOMPARE(logic.cell(0, 2), 2);
    QCOMPARE(logic.cell(1, 1), 2);
    QCOMPARE(logic.cell(1, 2), 2);
}

void TestGames::testTetrisMoveLeftBlockedByWall()
{
    TetrisLogic logic;
    logic.setPiece(0, 0, 0, 5);        // I 水平, x=0
    QVERIFY(!logic.moveLeft());        // x=-1 → 越界
    QCOMPARE(logic.x(), 0);
    QVERIFY(logic.moveRight());
    QCOMPARE(logic.x(), 1);
}

void TestGames::testTetrisHardDropLands()
{
    TetrisLogic logic;
    logic.setPiece(1, 0, 0, 0);        // O 方块从顶部
    logic.hardDrop();
    // 落到底部两行: (kRows-2,1),(kRows-2,2),(kRows-1,1),(kRows-1,2) = 2
    QCOMPARE(logic.cell(TetrisLogic::kRows - 1, 1), 2);
    QCOMPARE(logic.cell(TetrisLogic::kRows - 1, 2), 2);
    QCOMPARE(logic.cell(TetrisLogic::kRows - 2, 1), 2);
    QCOMPARE(logic.cell(TetrisLogic::kRows - 2, 2), 2);
}

QTEST_APPLESS_MAIN(TestGames)

#include "tst_games.moc"
