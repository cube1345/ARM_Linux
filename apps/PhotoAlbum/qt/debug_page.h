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

private slots:
    void refresh();

private:
    QLabel *m_textLabel;
    QTimer *m_timer;
    quint64 m_lastCpuTotal;
    quint64 m_lastCpuIdle;
    quint64 m_lastNetRx;
    quint64 m_lastNetTx;

    static QString readFile(const QString &path);
    int cpuUsagePercent();
    QString memText() const;
    QString netText();
    QString tempText() const;
    QString freqText() const;
    QString selfMemText() const;
};

#endif
