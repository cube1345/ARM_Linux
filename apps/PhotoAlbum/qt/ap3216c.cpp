#include "ap3216c.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static const quint8 kAddr = 0x1e;

Ap3216c::Ap3216c() : m_fd(-1)
{
}

Ap3216c::~Ap3216c()
{
    close();
}

bool Ap3216c::open(const QString &device)
{
    m_fd = ::open(device.toUtf8().constData(), O_RDWR);
    if (m_fd < 0)
        return false;
    if (ioctl(m_fd, I2C_SLAVE, kAddr) < 0) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    return true;
}

void Ap3216c::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool Ap3216c::writeReg(quint8 reg, quint8 val)
{
    if (m_fd < 0)
        return false;
    quint8 buf[2] = { reg, val };
    return ::write(m_fd, buf, 2) == 2;
}

bool Ap3216c::readRegs(quint8 reg, quint8 *buf, int len)
{
    if (m_fd < 0)
        return false;
    if (::write(m_fd, &reg, 1) != 1)
        return false;
    return ::read(m_fd, buf, len) == len;
}

bool Ap3216c::init()
{
    // 系统配置：使能 ALS（bit0）
    return writeReg(0x00, 0x01);
}

quint16 Ap3216c::readAls()
{
    quint8 buf[2] = { 0, 0 };
    if (!readRegs(0x0C, buf, 2))
        return 0;
    return quint16(buf[0]) | (quint16(buf[1]) << 8);
}
