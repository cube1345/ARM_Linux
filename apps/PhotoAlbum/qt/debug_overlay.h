#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <QLabel>

class QTimer;

class DebugOverlay : public QLabel
{
    Q_OBJECT
public:
    explicit DebugOverlay(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QTimer *m_timer;
};

#endif
