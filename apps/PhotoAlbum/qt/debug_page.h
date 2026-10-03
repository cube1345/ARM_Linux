#ifndef DEBUG_PAGE_H
#define DEBUG_PAGE_H

#include <QWidget>

class QLabel;
class QTimer;

class DebugPage : public QWidget
{
    Q_OBJECT
public:
    explicit DebugPage(QWidget *parent = nullptr);
    void setTouchInfo(int n, int x, int y);          // 触点（手势层喂入）
    void setImu(qreal ax, qreal ay, qreal az);       // IMU 加速度（m/s²）

private slots:
    void refresh();

private:
    QLabel *m_textLabel;
    QTimer *m_timer;
    quint64 m_lastCpuTotal;
    quint64 m_lastCpuIdle;
    quint64 m_lastNetRx;
    quint64 m_lastNetTx;
    int m_touchN = -1;
    int m_touchX = 0;
    int m_touchY = 0;
    qreal m_ax = 0.0, m_ay = 0.0, m_az = 0.0;

    static QString readFile(const QString &path);
    int cpuUsagePercent();
    QString memText() const;
    QString netText();
    QString tempText() const;
    QString freqText() const;
    QString selfMemText() const;
    QString storageText() const;
};

#endif
