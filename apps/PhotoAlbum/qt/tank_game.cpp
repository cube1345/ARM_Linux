#include "tank_game.h"

#include <QPainter>
#include <QTimer>

static const char *kMap[13] = {
    "#############",
    "#.....#.....#",
    "#BBBB.#.BBBB#",
    "#.....#.....#",
    "#..BBBBBBB..#",
    "#.....#.....#",
    "#.BBBB.BBBB.#",
    "#.....#.....#",
    "#..BBBBBBB..#",
    "#.....#.....#",
    "#...........#",
    "#...........#",
    "######E######",
};

TankGame::TankGame(QWidget *parent)
    : QWidget(parent),
      m_timer(new QTimer(this)),
      m_moveDir(DirNone),
      m_moveCounter(0),
      m_fireCooldown(0),
      m_enemyFireCounter(0),
      m_health(100),
      m_mana(100),
      m_level(1),
      m_enemyMoveInterval(6),
      m_enemyFireInterval(45)
{
    setMinimumSize(520, 520);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_timer, &QTimer::timeout, this, &TankGame::tick);
    resetMap();
    resetGame();
}

TankGame::~TankGame()
{
}

void TankGame::startGame()
{
    resetGame();
    m_timer->start(33);
}

void TankGame::stopGame()
{
    m_timer->stop();
}

void TankGame::setMoveDir(int dir)
{
    m_moveDir = dir;
}

void TankGame::setLevel(int level)
{
    m_level = qBound(1, level, 10);
    m_enemyMoveInterval = qMax(3, 10 - m_level);
    m_enemyFireInterval = qMax(20, 60 - m_level * 4);
}

void TankGame::fire()
{
    if (m_fireCooldown > 0 || !m_player.alive)
        return;
    if (m_bullets.size() > 6)
        return;
    Bullet b;
    b.pos = tankCenter(m_player);
    b.dir = m_player.dir;
    b.fromPlayer = true;
    m_bullets.append(b);
    m_fireCooldown = 18;
}

void TankGame::skillLaser()
{
    if (m_mana < 20 || !m_player.alive)
        return;
    m_mana -= 20;
    emit manaChanged(m_mana);

    Laser l;
    l.from = m_player.pos;
    l.dir = m_player.dir;
    l.life = 14;
    m_lasers.append(l);

    QPoint p = m_player.pos;
    while (true) {
        switch (m_player.dir) {
        case DirUp: p.ry() -= 1; break;
        case DirDown: p.ry() += 1; break;
        case DirLeft: p.rx() -= 1; break;
        case DirRight: p.rx() += 1; break;
        }
        if (p.x() < 0 || p.x() >= kGrid || p.y() < 0 || p.y() >= kGrid)
            break;
        if (m_map[p.y()][p.x()] == CellBrick)
            m_map[p.y()][p.x()] = CellEmpty;
        if (m_map[p.y()][p.x()] == CellBase) {
            m_map[p.y()][p.x()] = CellEmpty;
            break;
        }
        for (int e = 0; e < m_enemies.size(); ++e) {
            if (m_enemies[e].alive && m_enemies[e].pos == p)
                m_enemies[e].alive = false;
        }
    }
}

void TankGame::skillBomb()
{
    if (m_mana < 20 || !m_player.alive)
        return;
    m_mana -= 20;
    emit manaChanged(m_mana);

    QPoint c = m_player.pos;
    switch (m_player.dir) {
    case DirUp: c.ry() -= 5; break;
    case DirDown: c.ry() += 5; break;
    case DirLeft: c.rx() -= 5; break;
    case DirRight: c.rx() += 5; break;
    }
    c.setX(qBound(0, c.x(), kGrid - 1));
    c.setY(qBound(0, c.y(), kGrid - 1));

    Bomb b;
    b.center = c;
    b.life = 14;
    m_bombs.append(b);

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            const int r = c.y() + dr;
            const int col = c.x() + dc;
            if (r < 0 || r >= kGrid || col < 0 || col >= kGrid)
                continue;
            if (m_map[r][col] == CellBrick)
                m_map[r][col] = CellEmpty;
            for (int e = 0; e < m_enemies.size(); ++e) {
                if (m_enemies[e].alive && m_enemies[e].pos == QPoint(col, r))
                    m_enemies[e].alive = false;
            }
        }
    }
}

void TankGame::resetMap()
{
    m_map.clear();
    for (int r = 0; r < kGrid; ++r) {
        QVector<CellType> row;
        for (int c = 0; c < kGrid; ++c) {
            const char ch = kMap[r][c];
            if (ch == '#')
                row.append(CellSteel);
            else if (ch == 'B')
                row.append(CellBrick);
            else if (ch == 'E')
                row.append(CellBase);
            else
                row.append(CellEmpty);
        }
        m_map.append(row);
    }
}

void TankGame::resetGame()
{
    resetMap();
    m_bullets.clear();
    m_lasers.clear();
    m_bombs.clear();
    m_player.pos = QPoint(1, 11);
    m_player.dir = DirUp;
    m_player.alive = true;

    m_enemies.clear();
    const int enemyCount = qMin(qMax((m_level + 1) / 2, 1), 5);
    const QPoint spawns[5] = { QPoint(1, 1), QPoint(3, 1), QPoint(6, 1), QPoint(9, 1), QPoint(11, 1) };
    for (int i = 0; i < enemyCount; ++i) {
        Tank t;
        t.pos = spawns[i];
        t.dir = DirDown;
        t.alive = true;
        m_enemies.append(t);
    }
    m_moveDir = DirNone;
    m_moveCounter = 0;
    m_fireCooldown = 0;
    m_enemyFireCounter = m_enemyFireInterval;
    m_health = 100;
    m_mana = 100;
    emit healthChanged(m_health);
    emit manaChanged(m_mana);
    update();
}

void TankGame::tick()
{
    if (m_fireCooldown > 0)
        --m_fireCooldown;

    for (int i = 0; i < m_lasers.size(); ++i) {
        if (--m_lasers[i].life <= 0)
            m_lasers.removeAt(i--);
    }
    for (int i = 0; i < m_bombs.size(); ++i) {
        if (--m_bombs[i].life <= 0)
            m_bombs.removeAt(i--);
    }

    if (m_moveDir != DirNone && m_player.alive) {
        ++m_moveCounter;
        if (m_moveCounter >= 4) {
            m_moveCounter = 0;
            const int oldDir = m_player.dir;
            m_player.dir = m_moveDir;
            if (tankCanMove(m_player))
                moveTank(m_player);
            else
                m_player.dir = oldDir;
        }
    } else {
        m_moveCounter = 0;
    }

    updateEnemies();
    moveBullets();
    checkBulletHits();
    checkGameOver();
    update();
}

void TankGame::damagePlayer()
{
    m_health -= 20;
    if (m_health < 0)
        m_health = 0;
    emit healthChanged(m_health);
    if (m_health <= 0)
        m_player.alive = false;
}

bool TankGame::cellBlocked(int row, int col) const
{
    if (row < 0 || row >= kGrid || col < 0 || col >= kGrid)
        return true;
    return m_map[row][col] != CellEmpty;
}

bool TankGame::tankCanMove(const Tank &tank) const
{
    QPoint next = tank.pos;
    switch (tank.dir) {
    case DirUp: next.ry() -= 1; break;
    case DirDown: next.ry() += 1; break;
    case DirLeft: next.rx() -= 1; break;
    case DirRight: next.rx() += 1; break;
    }
    if (cellBlocked(next.y(), next.x()))
        return false;
    if (m_player.alive && next == m_player.pos && &tank != &m_player)
        return false;
    for (int i = 0; i < m_enemies.size(); ++i) {
        if (m_enemies[i].alive && m_enemies[i].pos == next && &tank != &m_enemies[i])
            return false;
    }
    return true;
}

void TankGame::moveTank(Tank &tank)
{
    switch (tank.dir) {
    case DirUp: tank.pos.ry() -= 1; break;
    case DirDown: tank.pos.ry() += 1; break;
    case DirLeft: tank.pos.rx() -= 1; break;
    case DirRight: tank.pos.rx() += 1; break;
    }
}

void TankGame::updateEnemies()
{
    if (--m_enemyFireCounter <= 0) {
        m_enemyFireCounter = m_enemyFireInterval;
        for (int i = 0; i < m_enemies.size(); ++i) {
            if (!m_enemies[i].alive)
                continue;
            Bullet b;
            b.pos = tankCenter(m_enemies[i]);
            b.dir = m_enemies[i].dir;
            b.fromPlayer = false;
            m_bullets.append(b);
        }
    }

    static int enemyMoveTick = 0;
    if (++enemyMoveTick < m_enemyMoveInterval)
        return;
    enemyMoveTick = 0;

    for (int i = 0; i < m_enemies.size(); ++i) {
        if (!m_enemies[i].alive)
            continue;
        if ((qrand() % 5) == 0)
            m_enemies[i].dir = qrand() % 4;
        if (tankCanMove(m_enemies[i]))
            moveTank(m_enemies[i]);
        else
            m_enemies[i].dir = (m_enemies[i].dir + 1) % 4;
    }
}

void TankGame::moveBullets()
{
    for (int i = 0; i < m_bullets.size(); ++i) {
        Bullet &b = m_bullets[i];
        const float speed = 3.0f;
        switch (b.dir) {
        case DirUp: b.pos.ry() -= speed; break;
        case DirDown: b.pos.ry() += speed; break;
        case DirLeft: b.pos.rx() -= speed; break;
        case DirRight: b.pos.rx() += speed; break;
        }
    }
}

void TankGame::checkBulletHits()
{
    for (int i = m_bullets.size() - 1; i >= 0; --i) {
        Bullet &b = m_bullets[i];
        const QPoint cell = tankAt(b.pos);
        if (cell.x() < 0 || cell.x() >= kGrid || cell.y() < 0 || cell.y() >= kGrid) {
            m_bullets.removeAt(i);
            continue;
        }
        const CellType t = m_map[cell.y()][cell.x()];
        if (t == CellBrick) {
            m_map[cell.y()][cell.x()] = CellEmpty;
            m_bullets.removeAt(i);
            continue;
        }
        if (t == CellSteel || t == CellBase) {
            m_bullets.removeAt(i);
            continue;
        }
        if (b.fromPlayer) {
            bool hit = false;
            for (int e = 0; e < m_enemies.size(); ++e) {
                if (m_enemies[e].alive && m_enemies[e].pos == cell) {
                    m_enemies[e].alive = false;
                    hit = true;
                    break;
                }
            }
            if (hit) {
                m_bullets.removeAt(i);
                continue;
            }
        } else if (m_player.alive && m_player.pos == cell) {
            damagePlayer();
            m_bullets.removeAt(i);
            continue;
        }
    }
}

void TankGame::checkGameOver()
{
    bool anyEnemy = false;
    for (int i = 0; i < m_enemies.size(); ++i) {
        if (m_enemies[i].alive) {
            anyEnemy = true;
            break;
        }
    }
    if (!anyEnemy) {
        m_timer->stop();
        emit gameOver(true);
        return;
    }
    if (!m_player.alive || m_map[12][6] != CellBase) {
        m_timer->stop();
        emit gameOver(false);
    }
}

QRect TankGame::cellRect(int row, int col) const
{
    const int ox = (width() - kGrid * kCell) / 2;
    const int oy = (height() - kGrid * kCell) / 2;
    return QRect(ox + col * kCell, oy + row * kCell, kCell, kCell);
}

QPointF TankGame::tankCenter(const Tank &tank) const
{
    const QRect r = cellRect(tank.pos.y(), tank.pos.x());
    return QPointF(r.center());
}

QPoint TankGame::tankAt(const QPointF &pixel) const
{
    const int ox = (width() - kGrid * kCell) / 2;
    const int oy = (height() - kGrid * kCell) / 2;
    return QPoint((pixel.x() - ox) / kCell, (pixel.y() - oy) / kCell);
}

void TankGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0b, 0x0f));

    for (int r = 0; r < kGrid; ++r) {
        for (int c = 0; c < kGrid; ++c) {
            const QRect cr = cellRect(r, c);
            if (m_map[r][c] == CellBrick) {
                p.fillRect(cr.adjusted(2, 2, -2, -2), QColor(0xb5, 0x6a, 0x3a));
                p.setPen(QColor(0x8a, 0x4a, 0x22));
                p.drawRect(cr.adjusted(2, 2, -2, -2));
            } else if (m_map[r][c] == CellSteel) {
                p.fillRect(cr.adjusted(1, 1, -1, -1), QColor(0x9a, 0x9a, 0x9a));
            } else if (m_map[r][c] == CellBase) {
                drawBase(p, r, c);
            }
        }
    }

    for (int i = 0; i < m_bombs.size(); ++i) {
        const Bomb &b = m_bombs[i];
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                const int r = b.center.y() + dr;
                const int c = b.center.x() + dc;
                if (r < 0 || r >= kGrid || c < 0 || c >= kGrid)
                    continue;
                p.fillRect(cellRect(r, c).adjusted(2, 2, -2, -2), QColor(0xff, 0x80, 0x30));
            }
        }
    }

    for (int i = 0; i < m_lasers.size(); ++i) {
        const Laser &l = m_lasers[i];
        const QRect from = cellRect(l.from.y(), l.from.x());
        const QPointF start = QPointF(from.center());
        QPointF end = start;
        const int far = kGrid * kCell;
        switch (l.dir) {
        case DirUp: end.ry() -= far; break;
        case DirDown: end.ry() += far; break;
        case DirLeft: end.rx() -= far; break;
        case DirRight: end.rx() += far; break;
        }
        QPen pen(QColor(0xff, 0x40, 0x40));
        pen.setWidth(6);
        p.setPen(pen);
        p.drawLine(start, end);
    }

    for (int i = 0; i < m_bullets.size(); ++i) {
        const Bullet &b = m_bullets[i];
        p.setBrush(b.fromPlayer ? QColor(0xff, 0xe0, 0x60) : QColor(0xff, 0x60, 0x60));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(b.pos).toPoint(), 3, 3);
    }

    if (m_player.alive)
        drawTank(p, tankCenter(m_player), m_player.dir, QColor(0x3a, 0xc0, 0x4a));
    for (int i = 0; i < m_enemies.size(); ++i) {
        if (m_enemies[i].alive)
            drawTank(p, tankCenter(m_enemies[i]), m_enemies[i].dir, QColor(0xd0, 0x3a, 0x3a));
    }
}

void TankGame::drawTank(QPainter &p, const QPointF &center, int dir, const QColor &c)
{
    p.save();
    p.translate(center);
    switch (dir) {
    case DirUp: break;
    case DirDown: p.rotate(180); break;
    case DirLeft: p.rotate(-90); break;
    case DirRight: p.rotate(90); break;
    }
    const int s = kCell - 8;
    const int half = s / 2;
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRect(-half, -half, s, s);
    p.setBrush(c.darker(130));
    p.drawRect(-half / 2, -half, half, s);
    p.setBrush(QColor(0x20, 0x20, 0x20));
    p.drawRect(-3, -half - 4, 6, 6);
    p.restore();
}

void TankGame::drawBase(QPainter &p, int row, int col)
{
    const QRect cr = cellRect(row, col);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xe0, 0xc0, 0x40));
    p.drawRect(cr.adjusted(4, 4, -4, -4));
    p.setBrush(QColor(0xc0, 0x30, 0x30));
    const int cx = cr.center().x();
    const int cy = cr.center().y();
    p.drawEllipse(QPoint(cx, cy), 10, 10);
}
