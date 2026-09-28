#ifndef AP3216C_H
#define AP3216C_H

#include <QString>

class Ap3216c
{
public:
    Ap3216c();
    ~Ap3216c();

    bool open(const QString &device = QStringLiteral("/dev/i2c-0"));
    void close();
    bool init();
    quint16 readAls();

private:
    int m_fd;
    bool writeReg(quint8 reg, quint8 val);
    bool readRegs(quint8 reg, quint8 *buf, int len);
};

#endif
