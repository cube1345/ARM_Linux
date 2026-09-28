#ifndef DEBUG_MONITOR_H
#define DEBUG_MONITOR_H

#include <QString>

class DebugMonitor
{
public:
    static QString summary();

private:
    static quint64 s_lastCpuTotal;
    static quint64 s_lastCpuIdle;
    static quint64 s_lastNetRx;
    static quint64 s_lastNetTx;

    static QString readFile(const QString &path);
    static int cpuUsagePercent();
    static QString memText();
    static QString netText();
    static QString tempText();
    static QString freqText();
    static QString selfMemText();
};

#endif
