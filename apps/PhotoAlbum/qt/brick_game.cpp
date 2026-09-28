#include "brick_game.h"

#include <QPainter>
#include <QTimer>

BrickGame::BrickGame(QWidget *parent)
    : QWidget(parent),
      m_paddleX(0),
      m_paddleDir(0),
      m_score(0),
      m_alive(true),
      m_timer(new QTimer(this))
{
    setMinimumSize(720, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_timer, &QTimer::timeout, this, &BrickGame::tick);
    resetGame();
}

BrickGame::~BrickGame()
{
}

void BrickGame::startGame()
{
    resetGame();
    m_timer->start(16);
}

void BrickGame::stopGame()
{
    m_timer->stop();
}

void BrickGame::setPaddleDir(int dir)
{
    m_paddleDir = dir;
}

void BrickGame::resetGame()
{
    m_bricks = QVector<QVector<bool> >(kBrickRows, QVector<bool>(kBrickCols, true));
    m_paddleX = (720 - kPaddleW) / 2.0f;
    m_paddleDir = 0;
    m_ball = QPointF(360, 350);
    m_vel = QPointF(3.0f, -3.5f);
    m_score = 0;
    m_alive = true;
    update();
}

QRect BrickGame::brickRect(int row, int col) const
{
    const int ox = (width() - kBrickCols * kBrickW) / 2;
    const int oy = 20;
    return QRect(ox + col * kBrickW, oy + row * kBrickH, kBrickW, kBrickH);
}

QRectF BrickGame::paddleRect() const
{
    const int y = height() - 40;
    return QRectF(m_paddleX, y, kPaddleW, kPaddleH);
}

void BrickGame::tick()
{
    if (!m_alive)
        return;

    // 挡板移动
    const float paddleSpeed = 8.0f;
    m_paddleX += m_paddleDir * paddleSpeed;
    m_paddleX = qBound(0.0f, m_paddleX, float(width() - kPaddleW));

    // 球移动
    m_ball += m_vel;

    // 撞墙反弹
    if (m_ball.x() < 4 || m_ball.x() > width() - 4)
        m_vel.setX(-m_vel.x());
    if (m_ball.y() < 4)
        m_vel.setY(-m_vel.y());

    // 掉底部失败
    if (m_ball.y() > height()) {
        m_alive = false;
        m_timer->stop();
        emit gameOver(false, m_score);
        update();
        return;
    }

    // 撞挡板反弹
    const QRectF pr = paddleRect();
    if (m_ball.y() > pr.top() - 6 && m_ball.y() < pr.bottom() &&
        m_ball.x() > pr.left() - 6 && m_ball.x() < pr.right() + 6) {
        const float hit = (m_ball.x() - pr.center().x()) / (kPaddleW / 2.0f);
        m_vel.setX(hit * 5.0f);
        m_vel.setY(-qAbs(m_vel.y()));
    }

    // 撞砖块
    for (int r = 0; r < kBrickRows; ++r) {
        for (int c = 0; c < kBrickCols; ++c) {
            if (!m_bricks[r][c])
                continue;
            const QRect br = brickRect(r, c);
            if (m_ball.x() > br.left() - 6 && m_ball.x() < br.right() + 6 &&
                m_ball.y() > br.top() - 6 && m_ball.y() < br.bottom() + 6) {
                m_bricks[r][c] = false;
                ++m_score;
                // 根据撞击方向反弹
                if (m_ball.y() < br.top() || m_ball.y() > br.bottom())
                    m_vel.setY(-m_vel.y());
                else
                    m_vel.setX(-m_vel.x());
            }
        }
    }

    // 胜利检查
    bool any = false;
    for (int r = 0; r < kBrickRows; ++r)
        for (int c = 0; c < kBrickCols; ++c)
            if (m_bricks[r][c])
                any = true;
    if (!any) {
        m_alive = false;
        m_timer->stop();
        emit gameOver(true, m_score);
    }

    update();
}

void BrickGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0b, 0x0f));

    for (int r = 0; r < kBrickRows; ++r) {
        for (int c = 0; c < kBrickCols; ++c) {
            if (!m_bricks[r][c])
                continue;
            const QRect br = brickRect(r, c);
            p.setPen(Qt::NoPen);
            p.setBrush(r % 2 == 0 ? QColor(0xd0, 0x60, 0x40) : QColor(0xe0, 0x90, 0x30));
            p.drawRect(br.adjusted(2, 2, -2, -2));
        }
    }

    p.setBrush(QColor(0x30, 0xa0, 0xe0));
    p.drawRect(paddleRect());

    p.setBrush(QColor(0xff, 0xff, 0xff));
    p.drawEllipse(m_ball.toPoint(), 6, 6);

    p.setPen(QColor(0xff, 0xff, 0xff));
    QFont f = p.font();
    f.setPixelSize(20);
    p.setFont(f);
    p.drawText(rect().adjusted(0, 0, -8, -8), Qt::AlignTop | Qt::AlignRight,
               QStringLiteral("得分 %1").arg(m_score));

    if (!m_alive) {
        f.setPixelSize(28);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter,
                   m_score >= kBrickRows * kBrickCols ? QStringLiteral("胜利！") : QStringLiteral("失败"));
    }
}
