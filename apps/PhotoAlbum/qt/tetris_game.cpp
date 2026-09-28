#include "tetris_game.h"

#include <QPainter>
#include <QTimer>

TetrisGame::TetrisGame(QWidget *parent)
    : QWidget(parent),
      m_timer(new QTimer(this))
{
    setMinimumSize(300, 520);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_timer, &QTimer::timeout, this, &TetrisGame::tick);
    m_logic.reset();
}

TetrisGame::~TetrisGame()
{
}

void TetrisGame::startGame()
{
    m_logic.reset();
    m_timer->start(500);
    update();
}

void TetrisGame::stopGame()
{
    m_timer->stop();
}

void TetrisGame::moveLeft()
{
    m_logic.moveLeft();
    update();
}

void TetrisGame::moveRight()
{
    m_logic.moveRight();
    update();
}

void TetrisGame::rotate()
{
    m_logic.rotate();
    update();
}

void TetrisGame::softDrop()
{
    m_logic.softDrop();
    update();
}

void TetrisGame::hardDrop()
{
    m_logic.hardDrop();
    if (!m_logic.alive()) {
        m_timer->stop();
        emit gameOver(m_logic.score());
    }
    update();
}

void TetrisGame::tick()
{
    m_logic.tick();
    if (!m_logic.alive()) {
        m_timer->stop();
        emit gameOver(m_logic.score());
    }
    update();
}

QRect TetrisGame::cellRect(int row, int col) const
{
    const int ox = (width() - TetrisLogic::kCols * kCell) / 2;
    const int oy = (height() - TetrisLogic::kRows * kCell) / 2;
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

    for (int r = 0; r < TetrisLogic::kRows; ++r) {
        for (int c = 0; c < TetrisLogic::kCols; ++c) {
            const QRect cr = cellRect(r, c);
            if (m_logic.cell(r, c) != 0)
                p.fillRect(cr.adjusted(1, 1, -1, -1), colorOf(m_logic.cell(r, c)));
            else
                p.fillRect(cr.adjusted(1, 1, -1, -1), QColor(0x18, 0x18, 0x20));
        }
    }

    if (m_logic.alive()) {
        for (int i = 0; i < 4; ++i) {
            const int cx = m_logic.x() + TetrisLogic::shapeDx(m_logic.type(), m_logic.rot(), i);
            const int cy = m_logic.y() + TetrisLogic::shapeDy(m_logic.type(), m_logic.rot(), i);
            if (cy >= 0 && cy < TetrisLogic::kRows)
                p.fillRect(cellRect(cy, cx).adjusted(1, 1, -1, -1), colorOf(m_logic.type() + 1));
        }
    }

    p.setPen(QColor(0xff, 0xff, 0xff));
    QFont f = p.font();
    f.setPixelSize(20);
    p.setFont(f);
    p.drawText(rect().adjusted(0, 0, -8, -8), Qt::AlignTop | Qt::AlignRight,
               QStringLiteral("得分 %1").arg(m_logic.score()));

    if (!m_logic.alive()) {
        f.setPixelSize(28);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("游戏结束  得分 %1").arg(m_logic.score()));
    }
}
