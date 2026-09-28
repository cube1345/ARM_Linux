#ifndef ICM20608_H
#define ICM20608_H

#include <QString>
#include <QVector3D>

class Icm20608
{
public:
    Icm20608();
    ~Icm20608();

    bool open(const QString &device = QStringLiteral("/dev/spidev2.0"));
    void close();
    bool isOpen() const { return m_fd >= 0; }

    quint8 readReg(quint8 reg);
    void writeReg(quint8 reg, quint8 value);

    bool init();
    quint8 whoAmI();
    QVector3D readAccel();

private:
    int m_fd;
};

#endif
