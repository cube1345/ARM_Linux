#include "debug_page.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QRegExp>
#include <QStorageInfo>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>

DebugPage::DebugPage(QWidget *parent)
    : QWidget(parent),
      m_textLabel(new QLabel(this)),
      m_timer(new QTimer(this)),
      m_lastCpuTotal(0),
      m_lastCpuIdle(0),
      m_lastNetRx(0),
      m_lastNetTx(0)
{
    m_textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    QFont f = m_textLabel->font();
    f.setPixelSize(20);
    m_textLabel->setFont(f);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->addWidget(m_textLabel);

    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &DebugPage::refresh);
    m_timer->start();
    refresh();
}

void DebugPage::setTouchInfo(int n, int x, int y)
{
    m_touchN = n;
    m_touchX = x;
    m_touchY = y;
}

void DebugPage::setImu(qreal ax, qreal ay, qreal az)
{
    m_ax = ax;
    m_ay = ay;
    m_az = az;
}

QString DebugPage::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

int DebugPage::cpuUsagePercent()
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
    if (m_lastCpuTotal == 0) {
        m_lastCpuTotal = total;
        m_lastCpuIdle = idle;
        return -1;
    }
    const quint64 dTotal = total - m_lastCpuTotal;
    const quint64 dIdle = idle - m_lastCpuIdle;
    m_lastCpuTotal = total;
    m_lastCpuIdle = idle;
    if (dTotal == 0)
        return 0;
    return int(100 - dIdle * 100 / dTotal);
}

QString DebugPage::memText() const
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
    return QStringLiteral("内存: %1 / %2 MB  (%3%)")
        .arg((total - avail) / 1024).arg(total / 1024).arg(usedPct);
}

QString DebugPage::netText()
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
    QString text = QStringLiteral("网络: 收 %1 KB  发 %2 KB")
        .arg(rx / 1024).arg(tx / 1024);
    if (m_lastNetRx > 0) {
        const double rxRate = (rx - m_lastNetRx) / 1024.0;
        const double txRate = (tx - m_lastNetTx) / 1024.0;
        text += QStringLiteral("  (↓%1 ↑%2 KB/s)")
            .arg(rxRate, 0, 'f', 1).arg(txRate, 0, 'f', 1);
    }
    m_lastNetRx = rx;
    m_lastNetTx = tx;
    return text;
}

QString DebugPage::tempText() const
{
    const QString temp = readFile(QStringLiteral("/sys/class/thermal/thermal_zone0/temp")).trimmed();
    if (temp.isEmpty())
        return QStringLiteral("温度: N/A");
    return QStringLiteral("温度: %1°C").arg(temp.toDouble() / 1000.0, 0, 'f', 1);
}

QString DebugPage::freqText() const
{
    const QString freq = readFile(QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq")).trimmed();
    if (freq.isEmpty())
        return QStringLiteral("频率: N/A");
    return QStringLiteral("频率: %1 MHz").arg(freq.toULongLong() / 1000);
}

QString DebugPage::selfMemText() const
{
    const QString status = readFile(QStringLiteral("/proc/self/status"));
    foreach (const QString &line, status.split(QLatin1Char('\n'))) {
        if (line.startsWith(QStringLiteral("VmRSS:"))) {
            const QStringList parts = line.split(QRegExp(QStringLiteral("\\s+")), QString::SkipEmptyParts);
            if (parts.size() >= 2)
                return QStringLiteral("本进程内存: %1 kB").arg(parts[1]);
        }
    }
    return QStringLiteral("本进程内存: N/A");
}

QString DebugPage::storageText() const
{
    const QStorageInfo s(QDir::rootPath());
    if (!s.isValid() || s.bytesTotal() <= 0)
        return QStringLiteral("存储: N/A");
    const qint64 used = s.bytesTotal() - s.bytesAvailable();
    return QStringLiteral("存储: %1 / %2 MB (%3%)")
        .arg(used / 1048576).arg(s.bytesTotal() / 1048576)
        .arg(int(used * 100 / s.bytesTotal()));
}

void DebugPage::refresh()
{
    const int cpu = cpuUsagePercent();
    const QString load = readFile(QStringLiteral("/proc/loadavg")).trimmed();
    QString text;
    text += QStringLiteral("CPU 占用: %1\n").arg(cpu >= 0 ? QString::number(cpu) : QStringLiteral("--"));
    text += freqText() + QLatin1Char('\n');
    text += QStringLiteral("负载(1/5/15): %1\n").arg(load.isEmpty() ? QStringLiteral("N/A") : load);
    text += memText() + QLatin1Char('\n');
    text += selfMemText() + QLatin1Char('\n');
    text += netText() + QLatin1Char('\n');
    text += tempText() + QLatin1Char('\n');
    text += storageText() + QLatin1Char('\n');
    // 触点（由 MainWindow 手势层实时喂入）
    if (m_touchN >= 0)
        text += QStringLiteral("触点: %1 (%2,%3)\n").arg(m_touchN).arg(m_touchX).arg(m_touchY);
    // IMU 加速度 -> 姿态
    text += QStringLiteral("IMU: %1 %2 %3 m/s²\n")
        .arg(m_ax, 0, 'f', 1).arg(m_ay, 0, 'f', 1).arg(m_az, 0, 'f', 1);
    const qreal g = qSqrt(qreal(m_ax * m_ax + m_ay * m_ay + m_az * m_az));
    if (g > 0.1) {
        const qreal roll = qRadiansToDegrees(qAtan2(m_ay, m_az));
        const qreal pitch = qRadiansToDegrees(
            qAtan2(-m_ax, qSqrt(qreal(m_ay * m_ay + m_az * m_az))));
        text += QStringLiteral("姿态 俯仰/横滚: %1° / %2°\n")
            .arg(pitch, 0, 'f', 1).arg(roll, 0, 'f', 1);
    }
    m_textLabel->setText(text);
}
