#include "tetris_logic.h"

#include <QtGlobal>

const int TetrisLogic::kShapes[7][4][4][2] = {
    // I
    { {{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
      {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}} },
    // O
    { {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}},
      {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}} },
    // T
    { {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
      {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}} },
    // S
    { {{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
      {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}} },
    // Z
    { {{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
      {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}} },
    // J
    { {{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
      {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}} },
    // L
    { {{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
      {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}} },
};

TetrisLogic::TetrisLogic()
    : m_type(0), m_rot(0), m_x(3), m_y(0), m_score(0), m_alive(true)
{
    reset();
}

void TetrisLogic::reset()
{
    m_board = QVector<QVector<int> >(kRows, QVector<int>(kCols, 0));
    m_score = 0;
    m_alive = true;
    newPiece();
}

int TetrisLogic::shapeDx(int type, int rot, int i) { return kShapes[type][rot][i][0]; }
int TetrisLogic::shapeDy(int type, int rot, int i) { return kShapes[type][rot][i][1]; }

int TetrisLogic::cell(int r, int c) const
{
    if (r < 0 || r >= kRows || c < 0 || c >= kCols)
        return 0;
    return m_board[r][c];
}

void TetrisLogic::setCell(int r, int c, int value)
{
    if (r >= 0 && r < kRows && c >= 0 && c < kCols)
        m_board[r][c] = value;
}

void TetrisLogic::setPiece(int type, int rot, int x, int y)
{
    m_type = type;
    m_rot = rot;
    m_x = x;
    m_y = y;
}

void TetrisLogic::newPiece()
{
    m_type = qrand() % 7;
    m_rot = 0;
    m_x = 3;
    m_y = 0;
    if (collides(m_type, m_rot, m_x, m_y))
        m_alive = false;
}

bool TetrisLogic::collides(int type, int rot, int x, int y) const
{
    for (int i = 0; i < 4; ++i) {
        const int cx = x + kShapes[type][rot][i][0];
        const int cy = y + kShapes[type][rot][i][1];
        if (cx < 0 || cx >= kCols || cy >= kRows)
            return true;
        if (cy >= 0 && m_board[cy][cx] != 0)
            return true;
    }
    return false;
}

bool TetrisLogic::moveLeft()
{
    if (!m_alive)
        return false;
    if (collides(m_type, m_rot, m_x - 1, m_y))
        return false;
    --m_x;
    return true;
}

bool TetrisLogic::moveRight()
{
    if (!m_alive)
        return false;
    if (collides(m_type, m_rot, m_x + 1, m_y))
        return false;
    ++m_x;
    return true;
}

bool TetrisLogic::rotate()
{
    if (!m_alive)
        return false;
    const int r = (m_rot + 1) % 4;
    if (collides(m_type, r, m_x, m_y))
        return false;
    m_rot = r;
    return true;
}

bool TetrisLogic::softDrop()
{
    if (!m_alive)
        return false;
    if (collides(m_type, m_rot, m_x, m_y + 1))
        return false;
    ++m_y;
    return true;
}

int TetrisLogic::clearLines()
{
    int cleared = 0;
    for (int r = kRows - 1; r >= 0; --r) {
        bool full = true;
        for (int c = 0; c < kCols; ++c) {
            if (m_board[r][c] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            m_board.removeAt(r);
            m_board.prepend(QVector<int>(kCols, 0));
            m_score += 100;
            ++cleared;
            ++r;
        }
    }
    return cleared;
}

int TetrisLogic::lockPiece()
{
    for (int i = 0; i < 4; ++i) {
        const int cx = m_x + kShapes[m_type][m_rot][i][0];
        const int cy = m_y + kShapes[m_type][m_rot][i][1];
        if (cy >= 0 && cy < kRows && cx >= 0 && cx < kCols)
            m_board[cy][cx] = m_type + 1;
    }
    const int cleared = clearLines();
    newPiece();
    return cleared;
}

void TetrisLogic::hardDrop()
{
    if (!m_alive)
        return;
    while (!collides(m_type, m_rot, m_x, m_y + 1))
        ++m_y;
    lockPiece();
}

int TetrisLogic::tick()
{
    if (!m_alive)
        return 0;
    if (!collides(m_type, m_rot, m_x, m_y + 1)) {
        ++m_y;
        return 0;
    }
    return lockPiece();
}
