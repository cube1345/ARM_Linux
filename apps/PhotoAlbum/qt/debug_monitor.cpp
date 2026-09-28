#include "debug_monitor.h"

#include <QFile>
#include <QRegExp>

quint64 DebugMonitor::s_lastCpuTotal = 0;
quint64 DebugMonitor::s_lastCpuIdle = 0;
quint64 DebugMonitor::s_lastNetRx = 0;
quint64 DebugMonitor::s_lastNetTx = 0;

QString DebugMonitor::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

int DebugMonitor::cpuUsagePercent()
{
    const QString stat = readFile(QStringLiteral("/proc/stat"));
    const QStringList lines = stat.split(QLatin1Char('\n'));
    if (lines.isEmpty())
        return -1;
    const QStringList parts = lines.first().split(QRegExp(QStringLiteral("\\s+")),
                                                 QString::SkipEmptyParts);
    if (parts.size() < 5)
        return -1;
    quint64 total = 0;
    for (int i = 1; i < parts.size(); ++i)
        total += parts[i].toULongLong();
    quint64 idle = parts[4].toULongLong();
    if (parts.size() > 5)
        idle += parts[5].toULongLong();
    if (s_lastCpuTotal == 0) {
        s_lastCpuTotal = total;
        s_lastCpuIdle = idle;
        return -1;
    }
    const quint64 dTotal = total - s_lastCpuTotal;
    const quint64 dIdle = idle - s_lastCpuIdle;
    s_lastCpuTotal = total;
    s_lastCpuIdle = idle;
    if (dTotal == 0)
        return 0;
    return int(100 - dIdle * 100 / dTotal);
}

QString DebugMonitor::memText()
{
    const QString info = readFile(QStringLiteral("/proc/meminfo"));
    quint64 total = 0;
    quint64 avail = 0;
    foreach (const QString &line, info.split(QLatin1Char('\n'))) {
        const QStringList parts = line.split(QRegExp(QStringLiteral("\\s+")), QString::SkipEmptyParts);
        if (parts.size() < 2)
            continue;
        if (parts[0] == QStringLiteral("MemTotal:"))
            total = parts[1].toULongLong();
        else if (parts[0] == QStringLiteral("MemAvailable:"))
            avail = parts[1].toULongLong();
    }
    const int usedPct = total > 0 ? int(100 - avail * 100 / total) : 0;
    return QStringLiteral("%1/%2MB(%3%)").arg((total - avail) / 1024).arg(total / 1024).arg(usedPct);
}

QString DebugMonitor::netText()
{
    const QString info = readFile(QStringLiteral("/proc/net/dev"));
    quint64 rx = 0;
    quint64 tx = 0;
    foreach (const QString &line, info.split(QLatin1Char('\n'))) {
        if (!line.contains(QLatin1Char(':')))
            continue;
        const QStringList parts = line.split(QRegExp(QStringLiteral("\\s+")), QString::SkipEmptyParts);
        if (parts.size() < 10)
            continue;
        rx += parts[1].toULongLong();
        tx += parts[9].toULongLong();
    }
    QString text;
    if (s_lastNetRx > 0) {
        const double rxRate = (rx - s_lastNetRx) / 1024.0;
        const double txRate = (tx - s_lastNetTx) / 1024.0;
        text = QStringLiteral("↓%1 ↑%2KB/s").arg(rxRate, 0, 'f', 1).arg(txRate, 0, 'f', 1);
    } else {
        text = QStringLiteral("--");
    }
    s_lastNetRx = rx;
    s_lastNetTx = tx;
    return text;
}

QString DebugMonitor::tempText()
{
    const QString temp = readFile(QStringLiteral("/sys/class/thermal/thermal_zone0/temp")).trimmed();
    if (temp.isEmpty())
        return QStringLiteral("N/A");
    return QStringLiteral("%1°C").arg(temp.toDouble() / 1000.0, 0, 'f', 1);
}

QString DebugMonitor::freqText()
{
    const QString freq = readFile(QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq")).trimmed();
    if (freq.isEmpty())
        return QStringLiteral("N/A");
    return QStringLiteral("%1MHz").arg(freq.toULongLong() / 1000);
}

QString DebugMonitor::selfMemText()
{
    const QString status = readFile(QStringLiteral("/proc/self/status"));
    foreach (const QString &line, status.split(QLatin1Char('\n'))) {
        if (line.startsWith(QStringLiteral("VmRSS:"))) {
            const QStringList parts = line.split(QRegExp(QStringLiteral("\\s+")), QString::SkipEmptyParts);
            if (parts.size() >= 2)
                return QStringLiteral("%1kB").arg(parts[1]);
        }
    }
    return QStringLiteral("N/A");
}

QString DebugMonitor::summary()
{
    const int cpu = cpuUsagePercent();
    return QStringLiteral("CPU %1%  %2  MEM %3  NET %4  %5")
        .arg(cpu >= 0 ? QString::number(cpu) : QStringLiteral("--"))
        .arg(freqText())
        .arg(memText())
        .arg(netText())
        .arg(tempText());
}
