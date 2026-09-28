#include "icon_factory.h"

#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QtMath>

QPixmap IconFactory::album()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    QLinearGradient gradient(0, 0, 88, 88);
    gradient.setColorAt(0.0, QColor(58, 131, 255));
    gradient.setColorAt(1.0, QColor(164, 90, 255));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    painter.setPen(QPen(QColor(255, 255, 255, 235), 5,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(12, 66, 34, 42);
    painter.drawLine(34, 42, 46, 58);
    painter.drawLine(46, 58, 62, 36);
    painter.drawLine(62, 36, 76, 66);
    painter.drawEllipse(QPointF(64, 24), 5, 5);
    return icon;
}

QPixmap IconFactory::monitor()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setBrush(QColor(30, 42, 56));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    painter.setBrush(QColor(58, 131, 255));
    painter.drawRect(10, 14, 30, 22);
    painter.drawRect(48, 14, 30, 22);
    painter.drawRect(10, 52, 30, 22);
    painter.drawRect(48, 52, 30, 22);
    return icon;
}

QPixmap IconFactory::video()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setBrush(QColor(255, 59, 48));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    QPolygon triangle;
    triangle << QPoint(34, 26) << QPoint(66, 44) << QPoint(34, 62);
    painter.setBrush(Qt::white);
    painter.drawPolygon(triangle);
    return icon;
}

QPixmap IconFactory::game()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x3a, 0xc0, 0x4a));
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    painter.setBrush(QColor(0x20, 0x80, 0x30));
    painter.drawRect(22, 42, 44, 28);
    painter.setBrush(QColor(0x20, 0x20, 0x20));
    painter.drawRect(41, 32, 6, 20);
    painter.drawRect(18, 42, 52, 8);
    painter.drawRect(18, 62, 52, 8);
    return icon;
}

QPixmap IconFactory::snake()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x28, 0xa0, 0x38));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setBrush(QColor(0x3a, 0xd0, 0x4a));
    p.drawEllipse(18, 40, 14, 14);
    p.drawEllipse(32, 40, 14, 14);
    p.drawEllipse(46, 40, 14, 14);
    p.drawEllipse(60, 40, 14, 14);
    p.setBrush(QColor(0x20, 0x20, 0x20));
    p.drawEllipse(65, 43, 5, 5);
    return icon;
}

QPixmap IconFactory::tetris()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1e, 0x1e, 0x2c));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setBrush(QColor(0xe0, 0x40, 0x40));
    p.drawRect(26, 24, 18, 18);
    p.setBrush(QColor(0x30, 0xc0, 0xe0));
    p.drawRect(44, 24, 18, 18);
    p.setBrush(QColor(0x40, 0xd0, 0x60));
    p.drawRect(26, 42, 18, 18);
    p.setBrush(QColor(0xe0, 0xc0, 0x30));
    p.drawRect(44, 42, 18, 18);
    p.setBrush(QColor(0xa0, 0x50, 0xd0));
    p.drawRect(26, 60, 18, 18);
    return icon;
}

QPixmap IconFactory::brick()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1a, 0x1a, 0x24));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setBrush(QColor(0xd0, 0x60, 0x40));
    p.drawRect(16, 18, 18, 12);
    p.drawRect(38, 18, 18, 12);
    p.drawRect(60, 18, 14, 12);
    p.setBrush(QColor(0xe0, 0x90, 0x30));
    p.drawRect(16, 34, 18, 12);
    p.drawRect(38, 34, 18, 12);
    p.drawRect(60, 34, 14, 12);
    p.setBrush(QColor(0x30, 0xa0, 0xe0));
    p.drawRect(28, 62, 32, 8);
    p.setBrush(QColor(0xff, 0xff, 0xff));
    p.drawEllipse(40, 50, 10, 10);
    return icon;
}

QPixmap IconFactory::game2048()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x20, 0x20, 0x28));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setBrush(QColor(0xe0, 0x90, 0x40));
    p.drawRoundedRect(18, 26, 52, 36, 8, 8);
    p.setPen(QColor(0xff, 0xff, 0xff));
    QFont f = p.font();
    f.setPixelSize(22);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(18, 26, 52, 36), Qt::AlignCenter, QStringLiteral("2048"));
    return icon;
}

QPixmap IconFactory::music()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xa0, 0x50, 0xd0));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setBrush(QColor(0xff, 0xff, 0xff));
    p.drawEllipse(22, 48, 16, 12);
    p.drawRect(36, 28, 6, 32);
    p.drawEllipse(52, 48, 16, 12);
    p.drawRect(66, 28, 6, 32);
    p.drawRect(36, 28, 36, 6);
    return icon;
}

QPixmap IconFactory::calculator()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x30, 0x30, 0x40));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setBrush(QColor(0x20, 0x20, 0x28));
    p.drawRect(20, 20, 48, 20);
    p.setBrush(QColor(0xe0, 0xe0, 0xe0));
    p.drawRect(20, 48, 11, 10);
    p.drawRect(35, 48, 11, 10);
    p.drawRect(50, 48, 11, 10);
    p.setBrush(QColor(0x30, 0xa0, 0xe0));
    p.drawRect(20, 62, 11, 10);
    p.drawRect(35, 62, 11, 10);
    p.drawRect(50, 62, 11, 10);
    return icon;
}

QPixmap IconFactory::draw()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x40, 0x40, 0x50));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setPen(QPen(QColor(0xff, 0xff, 0xff), 8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(26, 62, 62, 26);
    p.setBrush(QColor(0xff, 0xff, 0xff));
    p.drawEllipse(QPoint(62, 26), 5, 5);
    return icon;
}

QPixmap IconFactory::settings()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x58, 0x58, 0x68));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * 3.1415926 / 4.0;
        const QPoint c(qRound(44 + 22 * qCos(angle)), qRound(44 + 22 * qSin(angle)));
        p.setBrush(QColor(0xd0, 0xd0, 0xd8));
        p.drawEllipse(c, 7, 7);
    }
    p.setBrush(QColor(0xd0, 0xd0, 0xd8));
    p.drawEllipse(QPoint(44, 44), 16, 16);
    p.setBrush(QColor(0x58, 0x58, 0x68));
    p.drawEllipse(QPoint(44, 44), 7, 7);
    return icon;
}

QPixmap IconFactory::debug()
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1e, 0x2a, 0x36));
    p.drawRoundedRect(0, 0, 88, 88, 20, 20);
    p.setPen(QPen(QColor(0x40, 0xd0, 0x80), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(QPolygon() << QPoint(16, 48) << QPoint(30, 36) << QPoint(42, 52)
                              << QPoint(56, 28) << QPoint(70, 44));
    p.setBrush(QColor(0x40, 0xd0, 0x80));
    p.drawEllipse(QPoint(70, 44), 4, 4);
    return icon;
}
