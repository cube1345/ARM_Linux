#include "tetris_game.h"

#include <QPainter>
#include <QTimer>

const int TetrisGame::kShapes[7][4][4][2] = {
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

TetrisGame::TetrisGame(QWidget *parent)
    : QWidget(parent),
      m_type(0),
      m_rot(0),
      m_x(3),
      m_y(0),
      m_score(0),
      m_alive(true),
      m_timer(new QTimer(this))
{
    setMinimumSize(300, 520);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_timer, &QTimer::timeout, this, &TetrisGame::tick);
    resetGame();
}

TetrisGame::~TetrisGame()
{
}

void TetrisGame::startGame()
{
    resetGame();
    m_timer->start(500);
}

void TetrisGame::stopGame()
{
    m_timer->stop();
}

void TetrisGame::resetGame()
{
    m_board = QVector<QVector<int> >(kRows, QVector<int>(kCols, 0));
    m_score = 0;
    m_alive = true;
    newPiece();
    update();
}

void TetrisGame::newPiece()
{
    m_type = qrand() % 7;
    m_rot = 0;
    m_x = 3;
    m_y = 0;
    if (collides(m_type, m_rot, m_x, m_y)) {
        m_alive = false;
        m_timer->stop();
        emit gameOver(m_score);
    }
}

bool TetrisGame::collides(int type, int rot, int x, int y) const
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

void TetrisGame::moveLeft()
{
    if (!m_alive) return;
    if (!collides(m_type, m_rot, m_x - 1, m_y))
        --m_x;
    update();
}

void TetrisGame::moveRight()
{
    if (!m_alive) return;
    if (!collides(m_type, m_rot, m_x + 1, m_y))
        ++m_x;
    update();
}

void TetrisGame::rotate()
{
    if (!m_alive) return;
    const int r = (m_rot + 1) % 4;
    if (!collides(m_type, r, m_x, m_y))
        m_rot = r;
    update();
}

void TetrisGame::softDrop()
{
    if (!m_alive) return;
    if (!collides(m_type, m_rot, m_x, m_y + 1))
        ++m_y;
    update();
}

void TetrisGame::hardDrop()
{
    if (!m_alive) return;
    while (!collides(m_type, m_rot, m_x, m_y + 1))
        ++m_y;
    lockPiece();
}

void TetrisGame::lockPiece()
{
    for (int i = 0; i < 4; ++i) {
        const int cx = m_x + kShapes[m_type][m_rot][i][0];
        const int cy = m_y + kShapes[m_type][m_rot][i][1];
        if (cy >= 0 && cy < kRows && cx >= 0 && cx < kCols)
            m_board[cy][cx] = m_type + 1;
    }
    clearLines();
    newPiece();
    update();
}

void TetrisGame::clearLines()
{
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
            ++r;
        }
    }
}

void TetrisGame::tick()
{
    if (!m_alive) return;
    if (!collides(m_type, m_rot, m_x, m_y + 1)) {
        ++m_y;
    } else {
        lockPiece();
    }
    update();
}

QRect TetrisGame::cellRect(int row, int col) const
{
    const int ox = (width() - kCols * kCell) / 2;
    const int oy = (height() - kRows * kCell) / 2;
    return QRect(ox + col * kCell, oy + row * kCell, kCell, kCell);
}

QColor TetrisGame::colorOf(int idx) const
{
    static const QColor colors[7] = {
        QColor(0x30, 0xc0, 0xe0), QColor(0xe0, 0xd0, 0x30),
        QColor(0xa0, 0x50, 0xd0), QColor(0x40, 0xd0, 0x60),
        QColor(0xe0, 0x40, 0x40), QColor(0x40, 0x60, 0xe0),
        QColor(0xe0, 0x90, 0x30)
    };
    if (idx <= 0 || idx > 7)
        return QColor(0x20, 0x20, 0x20);
    return colors[idx - 1];
}

void TetrisGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0b, 0x0f));

    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const QRect cr = cellRect(r, c);
            if (m_board[r][c] != 0) {
                p.fillRect(cr.adjusted(1, 1, -1, -1), colorOf(m_board[r][c]));
            } else {
                p.fillRect(cr.adjusted(1, 1, -1, -1), QColor(0x18, 0x18, 0x20));
            }
        }
    }

    if (m_alive) {
        for (int i = 0; i < 4; ++i) {
            const int cx = m_x + kShapes[m_type][m_rot][i][0];
            const int cy = m_y + kShapes[m_type][m_rot][i][1];
            if (cy >= 0 && cy < kRows)
                p.fillRect(cellRect(cy, cx).adjusted(1, 1, -1, -1), colorOf(m_type + 1));
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
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("游戏结束  得分 %1").arg(m_score));
    }
}
