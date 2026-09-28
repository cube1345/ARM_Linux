#include "game2048.h"

#include <QPainter>

Game2048::Game2048(QWidget *parent)
    : QWidget(parent),
      m_score(0),
      m_alive(true)
{
    setMinimumSize(440, 440);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    resetGame();
}

void Game2048::startGame()
{
    resetGame();
}

void Game2048::resetGame()
{
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            m_board[r][c] = 0;
    m_score = 0;
    m_alive = true;
    spawnRandom();
    spawnRandom();
    update();
}

void Game2048::spawnRandom()
{
    int empties[16];
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

bool Game2048::slideLine(int line[4])
{
    int out[4] = { 0, 0, 0, 0 };
    int oi = 0;
    for (int i = 0; i < 4; ++i) {
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
    for (int i = 0; i < 4; ++i) {
        if (line[i] != out[i])
            moved = true;
        line[i] = out[i];
    }
    return moved;
}

void Game2048::move(int dir)
{
    if (!m_alive)
        return;
    bool moved = false;
    if (dir == DirLeft || dir == DirRight) {
        for (int r = 0; r < kSize; ++r) {
            int line[4];
            for (int c = 0; c < kSize; ++c)
                line[c] = (dir == DirLeft) ? m_board[r][c] : m_board[r][kSize - 1 - c];
            if (slideLine(line))
                moved = true;
            for (int c = 0; c < kSize; ++c)
                m_board[r][c] = (dir == DirLeft) ? line[c] : line[kSize - 1 - c];
        }
    } else {
        for (int c = 0; c < kSize; ++c) {
            int line[4];
            for (int r = 0; r < kSize; ++r)
                line[r] = (dir == DirUp) ? m_board[r][c] : m_board[kSize - 1 - r][c];
            if (slideLine(line))
                moved = true;
            for (int r = 0; r < kSize; ++r)
                m_board[r][c] = (dir == DirUp) ? line[r] : line[kSize - 1 - r];
        }
    }

    if (moved)
        spawnRandom();

    // 检查胜利
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            if (m_board[r][c] >= 2048) {
                m_alive = false;
                emit gameOver(true);
                update();
                return;
            }

    if (!canMove()) {
        m_alive = false;
        emit gameOver(false);
    }
    update();
}

bool Game2048::canMove() const
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

QColor Game2048::colorOf(int val) const
{
    switch (val) {
    case 0: return QColor(0x20, 0x20, 0x28);
    case 2: return QColor(0x40, 0x40, 0x50);
    case 4: return QColor(0x50, 0x50, 0x70);
    case 8: return QColor(0xe0, 0x90, 0x40);
    case 16: return QColor(0xe0, 0x70, 0x40);
    case 32: return QColor(0xd0, 0x50, 0x40);
    case 64: return QColor(0xc0, 0x30, 0x30);
    case 128: return QColor(0xe0, 0xc0, 0x40);
    case 256: return QColor(0xe0, 0xb0, 0x30);
    case 512: return QColor(0xe0, 0xa0, 0x20);
    case 1024: return QColor(0xd0, 0x90, 0x20);
    case 2048: return QColor(0xff, 0xd0, 0x30);
    default: return QColor(0xff, 0xe0, 0x60);
    }
}

void Game2048::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0b, 0x0f));

    const int gap = 10;
    const int cell = (qMin(width(), height()) - gap * (kSize + 1)) / kSize;
    const int ox = (width() - cell * kSize - gap * (kSize - 1)) / 2;
    const int oy = (height() - cell * kSize - gap * (kSize - 1)) / 2;

    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            const QRect rc(ox + c * (cell + gap), oy + r * (cell + gap), cell, cell);
            p.setPen(Qt::NoPen);
            p.setBrush(colorOf(m_board[r][c]));
            p.drawRoundedRect(rc, 8, 8);
            if (m_board[r][c] != 0) {
                p.setPen(QColor(0xff, 0xff, 0xff));
                QFont f = p.font();
                f.setPixelSize(cell / 2);
                f.setBold(true);
                p.setFont(f);
                p.drawText(rc, Qt::AlignCenter, QString::number(m_board[r][c]));
            }
        }
    }

    p.setPen(QColor(0xff, 0xff, 0xff));
    QFont f = p.font();
    f.setPixelSize(20);
    p.setFont(f);
    p.drawText(rect().adjusted(0, 0, -8, -8), Qt::AlignTop | Qt::AlignRight,
               QStringLiteral("得分 %1").arg(m_score));

    if (!m_alive) {
        f.setPixelSize(28);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("游戏结束"));
    }
}
