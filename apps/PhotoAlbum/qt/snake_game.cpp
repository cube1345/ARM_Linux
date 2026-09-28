#include "snake_game.h"

#include <QPainter>
#include <QTimer>

SnakeGame::SnakeGame(QWidget *parent)
    : QWidget(parent),
      m_dir(DirRight),
      m_nextDir(DirRight),
      m_score(0),
      m_alive(true),
      m_timer(new QTimer(this))
{
    setMinimumSize(520, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_timer, &QTimer::timeout, this, &SnakeGame::tick);
    resetGame();
}

SnakeGame::~SnakeGame()
{
}

void SnakeGame::startGame()
{
    resetGame();
    m_timer->start(160);
}

void SnakeGame::stopGame()
{
    m_timer->stop();
}

void SnakeGame::setDirection(int dir)
{
    // 禁止反向
    if ((m_dir == DirUp && dir == DirDown) || (m_dir == DirDown && dir == DirUp) ||
        (m_dir == DirLeft && dir == DirRight) || (m_dir == DirRight && dir == DirLeft))
        return;
    m_nextDir = dir;
}

void SnakeGame::resetGame()
{
    m_snake.clear();
    m_snake.append(QPoint(kCols / 2, kRows / 2));
    m_snake.append(QPoint(kCols / 2 - 1, kRows / 2));
    m_snake.append(QPoint(kCols / 2 - 2, kRows / 2));
    m_dir = DirRight;
    m_nextDir = DirRight;
    m_score = 0;
    m_alive = true;
    spawnFood();
    update();
}

void SnakeGame::spawnFood()
{
    do {
        m_food = QPoint(qrand() % kCols, qrand() % kRows);
    } while (m_snake.contains(m_food));
}

void SnakeGame::tick()
{
    if (!m_alive)
        return;
    m_dir = m_nextDir;
    QPoint head = m_snake.first();
    switch (m_dir) {
    case DirUp: head.ry() -= 1; break;
    case DirDown: head.ry() += 1; break;
    case DirLeft: head.rx() -= 1; break;
    case DirRight: head.rx() += 1; break;
    }
    if (head.x() < 0 || head.x() >= kCols || head.y() < 0 || head.y() >= kRows ||
        m_snake.contains(head)) {
        m_alive = false;
        m_timer->stop();
        emit gameOver(m_score);
        update();
        return;
    }
    m_snake.prepend(head);
    if (head == m_food) {
        ++m_score;
        spawnFood();
    } else {
        m_snake.removeLast();
    }
    update();
}

QRect SnakeGame::cellRect(int row, int col) const
{
    const int ox = (width() - kCols * kCell) / 2;
    const int oy = (height() - kRows * kCell) / 2;
    return QRect(ox + col * kCell, oy + row * kCell, kCell, kCell);
}

void SnakeGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0b, 0x0f));

    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x18, 0x18, 0x20));
            p.drawRect(cellRect(r, c).adjusted(1, 1, -1, -1));
        }
    }

    if (m_alive) {
        p.setBrush(QColor(0xff, 0x50, 0x50));
        p.drawEllipse(cellRect(m_food.y(), m_food.x()).adjusted(6, 6, -6, -6));
    }

    for (int i = 0; i < m_snake.size(); ++i) {
        const QPoint seg = m_snake.at(i);
        p.setBrush(i == 0 ? QColor(0x3a, 0xd0, 0x4a) : QColor(0x28, 0xa0, 0x38));
        p.drawRect(cellRect(seg.y(), seg.x()).adjusted(2, 2, -2, -2));
    }

    if (!m_alive) {
        p.setPen(QColor(0xff, 0xff, 0xff));
        QFont f = p.font();
        f.setPixelSize(28);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("游戏结束  得分 %1").arg(m_score));
    }
}
