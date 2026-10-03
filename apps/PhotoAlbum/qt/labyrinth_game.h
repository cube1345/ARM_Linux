#ifndef LABYRINTH_GAME_H
#define LABYRINTH_GAME_H

#include <QPointF>
#include <QRandomGenerator>
#include <QTimer>
#include <QVector>
#include <QWidget>

// 迷宫滚球：IMU 倾斜控制小球，目标为中央孔；滑出边界或掉入陷阱失败。
// 运动学：倾斜 → 与重力沿板分量成正比的加速度；存在静摩擦阈值（力不足则静止），
// 动起来后带阻尼（动摩擦/空气）。
class LabyrinthGame : public QWidget
{
    Q_OBJECT
public:
    explicit LabyrinthGame(QWidget *parent = nullptr);

    void startGame();
    void setAccel(qreal ax, qreal ay);   // IMU 加速度 (m/s²)
    void setAcceleration(qreal g) { m_g = g; }   // 设定基准加速度 m/s²

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void tick();
    void setupHoles();
    void resetBall();
    QRectF plateRect() const;
    qreal holeRadius() const;

    QVector<QPointF> m_holes;   // 陷阱孔（比例坐标 0..1）
    QPointF m_target;           // 目标孔（比例坐标）
    QPointF m_ball;
    QPointF m_vel;
    qreal m_ax = 0.0, m_ay = 0.0;
    qreal m_g = 9.8f;                  // 设定基准加速度(换算用)
    qreal m_pitch = 0.0f, m_roll = 0.0f; // 推算倾角(度)
    int m_state = 0;            // 0 进行中 / 1 成功 / 2 失败
    QTimer *m_timer;
};

#endif // LABYRINTH_GAME_H