#include "game2048_logic.h"

#include <QtGlobal>

Game2048Logic::Game2048Logic()
    : m_score(0)
{
    reset();
}

void Game2048Logic::reset()
{
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            m_board[r][c] = 0;
    m_score = 0;
    spawnRandom();
    spawnRandom();
}

int Game2048Logic::cell(int r, int c) const
{
    if (r < 0 || r >= kSize || c < 0 || c >= kSize)
        return 0;
    return m_board[r][c];
}

void Game2048Logic::setCell(int r, int c, int value)
{
    if (r >= 0 && r < kSize && c >= 0 && c < kSize)
        m_board[r][c] = value;
}

void Game2048Logic::spawnRandom()
{
    int empties[kSize * kSize];
    int n = 0;
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            if (m_board[r][c] == 0)
                empties[n++] = r * kSize + c;
    if (n == 0)
        return;
    const int idx = empties[qrand() % n];
    m_board[idx / kSize][idx % kSize] = (qrand() % 10 == 0) ? 4 : 2;
}

bool Game2048Logic::slideLine(int line[kSize])
{
    int out[kSize] = { 0 };
    int oi = 0;
    for (int i = 0; i < kSize; ++i) {
        if (line[i] == 0)
            continue;
        if (oi > 0 && out[oi - 1] == line[i]) {
            out[oi - 1] *= 2;
            m_score += out[oi - 1];
        } else {
            out[oi++] = line[i];
        }
    }
    bool moved = false;
    for (int i = 0; i < kSize; ++i) {
        if (line[i] != out[i])
            moved = true;
        line[i] = out[i];
    }
    return moved;
}

bool Game2048Logic::moveNoSpawn(int dir)
{
    bool moved = false;
    if (dir == DirLeft || dir == DirRight) {
        for (int r = 0; r < kSize; ++r) {
            int line[kSize];
            for (int c = 0; c < kSize; ++c)
                line[c] = (dir == DirLeft) ? m_board[r][c] : m_board[r][kSize - 1 - c];
            if (slideLine(line))
                moved = true;
            for (int c = 0; c < kSize; ++c)
                m_board[r][c] = (dir == DirLeft) ? line[c] : line[kSize - 1 - c];
        }
    } else {
        for (int c = 0; c < kSize; ++c) {
            int line[kSize];
            for (int r = 0; r < kSize; ++r)
                line[r] = (dir == DirUp) ? m_board[r][c] : m_board[kSize - 1 - r][c];
            if (slideLine(line))
                moved = true;
            for (int r = 0; r < kSize; ++r)
                m_board[r][c] = (dir == DirUp) ? line[r] : line[kSize - 1 - r];
        }
    }
    return moved;
}

bool Game2048Logic::move(int dir)
{
    const bool moved = moveNoSpawn(dir);
    if (moved)
        spawnRandom();
    return moved;
}

bool Game2048Logic::canMove() const
{
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            if (m_board[r][c] == 0)
                return true;
            if (c < kSize - 1 && m_board[r][c] == m_board[r][c + 1])
                return true;
            if (r < kSize - 1 && m_board[r][c] == m_board[r + 1][c])
                return true;
        }
    }
    return false;
}
