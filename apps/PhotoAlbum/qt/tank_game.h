#ifndef TANK_GAME_H
#define TANK_GAME_H

#include <QPoint>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QTimer;

class TankGame : public QWidget
{
    Q_OBJECT
public:
    explicit TankGame(QWidget *parent = nullptr);
    ~TankGame() override;

    enum Direction { DirUp = 0, DirDown, DirLeft, DirRight, DirNone = -1 };

    void startGame();
    void stopGame();
    void setMoveDir(int dir);
    void fire();
    void skillLaser();
    void skillBomb();

    int health() const { return m_health; }
    int mana() const { return m_mana; }
    void setLevel(int level);
    int level() const { return m_level; }

signals:
    void gameOver(bool win);
    void healthChanged(int health);
    void manaChanged(int mana);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    enum CellType { CellEmpty, CellBrick, CellSteel, CellBase };
    struct Tank {
        QPoint pos;
        int dir;
        bool alive;
    };
    struct Bullet {
        QPointF pos;
        int dir;
        bool fromPlayer;
    };
    struct Laser {
        QPoint from;
        int dir;
        int life;
    };
    struct Bomb {
        QPoint center;
        int life;
    };

    static const int kGrid = 13;
    static const int kCell = 40;

    QVector<QVector<CellType> > m_map;
    Tank m_player;
    QVector<Tank> m_enemies;
    QVector<Bullet> m_bullets;
    QVector<Laser> m_lasers;
    QVector<Bomb> m_bombs;

    QTimer *m_timer;
    int m_moveDir;
    int m_moveCounter;
    int m_fireCooldown;
    int m_enemyFireCounter;
    int m_health;
    int m_mana;
    int m_level;
    int m_enemyMoveInterval;
    int m_enemyFireInterval;

    void resetMap();
    void resetGame();
    bool cellBlocked(int row, int col) const;
    bool tankCanMove(const Tank &tank) const;
    void moveTank(Tank &tank);
    void updateEnemies();
    void moveBullets();
    void checkBulletHits();
    void checkGameOver();
    void damagePlayer();

    QRect cellRect(int row, int col) const;
    QPointF tankCenter(const Tank &tank) const;
    QPoint tankAt(const QPointF &pixel) const;
    void drawTank(QPainter &p, const QPointF &center, int dir, const QColor &c);
    void drawBase(QPainter &p, int row, int col);
};

#endif
