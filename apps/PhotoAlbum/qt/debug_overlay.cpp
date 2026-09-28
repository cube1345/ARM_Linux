#include "debug_overlay.h"

#include <QTimer>

#include "debug_monitor.h"

DebugOverlay::DebugOverlay(QWidget *parent)
    : QLabel(parent), m_timer(new QTimer(this))
{
    setStyleSheet(QStringLiteral("background:rgba(0,0,0,0.72);color:#30d158;font-size:14px;padding:4px 8px;border-radius:4px;"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &DebugOverlay::refresh);
    m_timer->start();
    refresh();
}

void DebugOverlay::refresh()
{
    setText(DebugMonitor::summary());
    adjustSize();
}
