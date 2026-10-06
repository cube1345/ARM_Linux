#include "labyrinth_game.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {
const qreal kDt = 0.03f;            // 帧间隔(秒)
const qreal kAccelMul = 90.0f;      // 加速度系数：px/s² 每 (m/s²) 重力分量
const qreal kStaticFriction = 0.35f; // 静摩擦阈值(m/s²)：倾斜力小于此则球静止
const qreal kDamping = 0.98f;      // 每帧速度阻尼(动摩擦/空气)
const qreal kStallV2 = 0.0005f;     // 速度平方阈值：低于视为静止
}

LabyrinthGame::LabyrinthGame(QWidget *parent)
    : QWidget(parent)
    , m_target(QPointF(0.5f, 0.5f))
    , m_timer(new QTimer(this))
{
    setMinimumSize(320, 320);
    m_timer->setInterval(int(kDt * 1000));
    connect(m_timer, &QTimer::timeout, this, &LabyrinthGame::tick);
    setupHoles();
}

void LabyrinthGame::setupHoles()
{
    // 陷阱孔（比例坐标），避开中央目标区和四周边界
    const QPointF holes[] = {
        QPointF(0.18f, 0.16f), QPointF(0.50f, 0.12f), QPointF(0.84f, 0.16f),
        QPointF(0.14f, 0.50f), QPointF(0.86f, 0.50f),
        QPointF(0.22f, 0.84f), QPointF(0.78f, 0.84f), QPointF(0.50f, 0.90f),
        QPointF(0.62f, 0.34f), QPointF(0.36f, 0.66f),
    };
    m_holes.clear();
    for (const QPointF &p : holes)
        m_holes.append(p);
}

QRectF LabyrinthGame::plateRect() const
{
    return QRectF(20.0, 20.0, width() - 40.0, height() - 40.0);
}

qreal LabyrinthGame::holeRadius() const
{
    const QRectF p = plateRect();
    return qMin(p.width(), p.height()) * 0.045;
}

void LabyrinthGame::resetBall()
{
    const QRectF plate = plateRect();
    const qreal r = holeRadius() * 2.2;
    const int triesMax = 60;
    for (int i = 0; i < triesMax; ++i) {
        const qreal x = plate.left() + r + int(QRandomGenerator::global()->bounded(
                          qMax<qreal>(1.0, plate.width() - 2 * r)));
        const qreal y = plate.top() + r + int(QRandomGenerator::global()->bounded(
                          qMax<qreal>(1.0, plate.height() - 2 * r)));
        const QPointF cand(x, y);
        bool ok = true;
        // 不出界、不在目标/陷阱孔内
        const QRectF zone = plate.adjusted(-4, -4, 4, 4);
        if (!zone.contains(cand))
            continue;
        const QPointF targetAbs(plate.left() + m_target.x() * plate.width(),
                                plate.top() + m_target.y() * plate.height());
        if ((cand - targetAbs).manhattanLength() < r)
            continue;
        for (const QPointF &h : m_holes) {
            const QPointF hp(plate.left() + h.x() * plate.width(),
                             plate.top() + h.y() * plate.height());
            const qreal dx = cand.x() - hp.x();
            const qreal dy = cand.y() - hp.y();
            if (dx * dx + dy * dy < r * r) {
                ok = false;
                break;
            }
        }
        if (ok) {
            m_ball = cand;
            m_vel = QPointF(0.0, 0.0);
            return;
        }
    }
    // 兜底：放中央偏离目标
    m_ball = plate.center();
    m_vel = QPointF(0.0, 0.0);
}

void LabyrinthGame::startGame()
{
    resetBall();
    m_state = 0;
    m_timer->start();
    update();
}

void LabyrinthGame::mouseReleaseEvent(QMouseEvent *event)
{
    // 结束态（成功/失败）点击屏幕重新开始
    if (m_state != 0) {
        startGame();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void LabyrinthGame::setAccel(qreal ax, qreal ay)
{
    m_ax = ax;
    m_ay = ay;
}

void LabyrinthGame::tick()
{
    if (m_state != 0) {
        m_timer->stop();
        return;
    }
    const QRectF plate = plateRect();

    // 依据 IMU 加速度推算倾角（忽略交叉耦合的近似）
    const qreal mag = qSqrt(m_ax * m_ax + m_ay * m_ay);
    const qreal gN = 9.81f;
    m_pitch = qRadiansToDegrees(qAsin(qBound<qreal>(-1.0, m_ax / gN, 1.0)));
    m_roll = -qRadiansToDegrees(qAsin(qBound<qreal>(-1.0, m_ay / gN, 1.0)));

    // 受力 = 设定加速度 × sin(倾角)，方向指向低处(-accel)
    // 注意：IMU 的 y 轴方向与屏幕 y 相反，故 y 分量取 +m_ay（否则运动方向与实际斜向相反）。
    QPointF ramp(0, 0);
    if (mag > 0.05 && mag < gN * 1.5f) {
        const qreal sinA = qMin<qreal>(1.0, mag / gN);
        ramp = QPointF(-m_ax / mag, m_ay / mag) * (m_g * sinA);
    }
    if (mag < kStaticFriction) {
        m_vel *= 0.86;
    } else {
        m_vel += ramp * (kAccelMul * kDt);
    }
    m_vel *= kDamping;
    if (mag < kStaticFriction &&
        m_vel.x() * m_vel.x() + m_vel.y() * m_vel.y() < kStallV2)
        m_vel = QPointF(0.0, 0.0);
    m_ball += m_vel * kDt;

    const qreal hr = holeRadius();
    const QPointF targetAbs(plate.left() + m_target.x() * plate.width(),
                            plate.top() + m_target.y() * plate.height());

    // 成功：进入目标孔
    if ((m_ball - targetAbs).manhattanLength() < hr) {
        m_state = 1;
        m_timer->stop();
        update();
        return;
    }
    // 失败：掉入陷阱
    for (const QPointF &h : m_holes) {
        const QPointF hp(plate.left() + h.x() * plate.width(),
                         plate.top() + h.y() * plate.height());
        if ((m_ball - hp).manhattanLength() < hr) {
            m_state = 2;
            m_timer->stop();
            update();
            return;
        }
    }
    // 失败：滑出边界
    if (!plate.adjusted(-6, -6, 6, 6).contains(m_ball)) {
        m_state = 2;
        m_timer->stop();
        update();
        return;
    }
    update();
}

void LabyrinthGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plate = plateRect();
    const qreal hr = holeRadius();
    const QPointF targetAbs(plate.left() + m_target.x() * plate.width(),
                            plate.top() + m_target.y() * plate.height());

    // 木纹盘底
    p.fillRect(rect(), QColor(0x2a, 0x1f, 0x17));
    QLinearGradient g(plate.topLeft(), plate.bottomRight());
    g.setColorAt(0.0, QColor(0xb8, 0x8c, 0x4a));
    g.setColorAt(1.0, QColor(0x8f, 0x6f, 0x38));
    p.setPen(QPen(QColor(0x5c, 0x46, 0x24), 3));
    p.setBrush(g);
    p.drawRoundedRect(plate, 14, 14);

    // 目标孔
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x16, 0x0f, 0x08));
    p.drawEllipse(targetAbs, hr, hr);
    p.setBrush(QColor(0x35, 0x28, 0x14));
    p.drawEllipse(targetAbs, hr * 0.55, hr * 0.55);

    // 陷阱孔
    for (const QPointF &h : m_holes) {
        const QPointF hp(plate.left() + h.x() * plate.width(),
                         plate.top() + h.y() * plate.height());
        p.setBrush(QColor(0x18, 0x10, 0x06));
        p.drawEllipse(hp, hr, hr);
    }

    // 小球
    p.setBrush(QColor(0xff, 0x45, 0x3a));
    p.setPen(QPen(QColor(0xff, 0xb3, 0xa0), 1.5));
    p.drawEllipse(m_ball, hr * 0.72, hr * 0.72);
    p.setBrush(QColor(255, 255, 255, 90));
    p.drawEllipse(m_ball - QPointF(hr * 0.22, hr * 0.22), hr * 0.2, hr * 0.2);

    // 实时数据条：设定加速度 / 倾角 / 实时速度
    {
        const QRect dbgRect(10, 8, 340, 68);
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 130));
        p.drawRoundedRect(dbgRect, 8, 8);
        p.setPen(Qt::white);
        QFont df = p.font();
        df.setPixelSize(14);
        p.setFont(df);
        p.drawText(dbgRect.adjusted(8, 6, -6, -6),
                    QStringLiteral("加速度设定 %1 m/s²\n倾角 x/y %2°  %3°\n实时速度 %4  %5 px/s")
                        .arg(m_g, 0, 'f', 1)
                        .arg(m_pitch, 0, 'f', 0).arg(m_roll, 0, 'f', 0)
                        .arg(m_vel.x(), 0, 'f', 0).arg(m_vel.y(), 0, 'f', 0));
        p.restore();
    }

    // 状态
    if (m_state != 0) {
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(28);
        f.setBold(true);
        p.setFont(f);
        const QString msg = m_state == 1 ? QStringLiteral("成功！进入目标孔")
                                         : QStringLiteral("失败：掉落/出界");
        p.drawText(rect(), Qt::AlignCenter, msg);
        QFont sf = p.font();
        sf.setPixelSize(16);
        sf.setBold(false);
        p.setFont(sf);
        p.drawText(rect().adjusted(0, 60, 0, 0), Qt::AlignHCenter | Qt::AlignCenter,
                   QStringLiteral("触摸屏幕重新开始"));
    }
}