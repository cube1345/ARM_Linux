#include "icm20608.h"

#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <linux/spi/spidev.h>

Icm20608::Icm20608() : m_fd(-1) {}

Icm20608::~Icm20608()
{
    close();
}

bool Icm20608::open(const QString &device)
{
    m_fd = ::open(device.toUtf8().constData(), O_RDWR);
    if (m_fd < 0) {
        qWarning() << "[ICM20608] open failed:" << device;
        return false;
    }
    quint8 mode = SPI_MODE_0;
    quint8 bits = 8;
    quint32 speed = 8000000;
    ioctl(m_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    return true;
}

void Icm20608::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

static bool spiTransfer(int fd, const quint8 *tx, quint8 *rx, int len)
{
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = 8000000;
    tr.bits_per_word = 8;
    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr) >= 0;
}

quint8 Icm20608::readReg(quint8 reg)
{
    quint8 tx[2] = { quint8(reg | 0x80), 0x00 };
    quint8 rx[2] = { 0, 0 };
    if (!spiTransfer(m_fd, tx, rx, 2))
        return 0;
    return rx[1];
}

void Icm20608::writeReg(quint8 reg, quint8 value)
{
    quint8 tx[2] = { reg, value };
    quint8 rx[2] = { 0, 0 };
    spiTransfer(m_fd, tx, rx, 2);
}

bool Icm20608::init()
{
    if (m_fd < 0)
        return false;
    writeReg(0x6B, 0x01);
    usleep(100000);
    writeReg(0x1C, 0x00);
    const quint8 id = whoAmI();
    qInfo().noquote() << QStringLiteral("[ICM20608] WHO_AM_I = 0x%1")
        .arg(id, 2, 16, QLatin1Char('0'));
    return (id == 0xAE || id == 0xAF);
}

quint8 Icm20608::whoAmI()
{
    return readReg(0x75);
}

QVector3D Icm20608::readAccel()
{
    quint8 tx[7] = { quint8(0x3B | 0x80), 0, 0, 0, 0, 0, 0 };
    quint8 rx[7] = { 0 };
    if (!spiTransfer(m_fd, tx, rx, 7))
        return QVector3D();
    const qint16 x = qint16((rx[1] << 8) | rx[2]);
    const qint16 y = qint16((rx[3] << 8) | rx[4]);
    const qint16 z = qint16((rx[5] << 8) | rx[6]);
    const float scale = 16384.0f;
    return QVector3D(x / scale, y / scale, z / scale);
}
