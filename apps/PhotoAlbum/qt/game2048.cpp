#include "game2048.h"

#include <QPainter>

Game2048::Game2048(QWidget *parent)
    : QWidget(parent),
      m_alive(true)
{
    setMinimumSize(440, 440);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    startGame();
}

void Game2048::startGame()
{
    m_logic.reset();
    m_alive = true;
    update();
}

void Game2048::move(int dir)
{
    if (!m_alive)
        return;
    m_logic.move(dir);

    for (int r = 0; r < Game2048Logic::kSize; ++r) {
        for (int c = 0; c < Game2048Logic::kSize; ++c) {
            if (m_logic.cell(r, c) >= 2048) {
                m_alive = false;
                emit gameOver(true);
                update();
                return;
            }
        }
    }

    if (!m_logic.canMove()) {
        m_alive = false;
        emit gameOver(false);
    }
    update();
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
    const int cell = (qMin(width(), height()) - gap * (Game2048Logic::kSize + 1)) / Game2048Logic::kSize;
    const int ox = (width() - cell * Game2048Logic::kSize - gap * (Game2048Logic::kSize - 1)) / 2;
    const int oy = (height() - cell * Game2048Logic::kSize - gap * (Game2048Logic::kSize - 1)) / 2;

    for (int r = 0; r < Game2048Logic::kSize; ++r) {
        for (int c = 0; c < Game2048Logic::kSize; ++c) {
            const int val = m_logic.cell(r, c);
            const QRect rc(ox + c * (cell + gap), oy + r * (cell + gap), cell, cell);
            p.setPen(Qt::NoPen);
            p.setBrush(colorOf(val));
            p.drawRoundedRect(rc, 8, 8);
            if (val != 0) {
                p.setPen(QColor(0xff, 0xff, 0xff));
                QFont f = p.font();
                f.setPixelSize(cell / 2);
                f.setBold(true);
                p.setFont(f);
                p.drawText(rc, Qt::AlignCenter, QString::number(val));
            }
        }
    }

    p.setPen(QColor(0xff, 0xff, 0xff));
    QFont f = p.font();
    f.setPixelSize(20);
    p.setFont(f);
    p.drawText(rect().adjusted(0, 0, -8, -8), Qt::AlignTop | Qt::AlignRight,
               QStringLiteral("得分 %1").arg(m_logic.score()));

    if (!m_alive) {
        f.setPixelSize(28);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("游戏结束"));
    }
}
