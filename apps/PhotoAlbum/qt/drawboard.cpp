#include "drawboard.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>

DrawBoard::DrawBoard(QWidget *parent)
    : QWidget(parent), m_drawing(false)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ensureImage();
}

void DrawBoard::ensureImage()
{
    if (m_image.isNull() || m_image.size() != size()) {
        QImage img(size(), QImage::Format_RGB32);
        img.fill(Qt::white);
        if (!m_image.isNull()) {
            QPainter p(&img);
            p.drawImage(0, 0, m_image);
        }
        m_image = img;
    }
}

void DrawBoard::clear()
{
    m_image.fill(Qt::white);
    update();
}

void DrawBoard::save()
{
    if (m_image.isNull())
        return;
    const QString dir = QStringLiteral("/home/root/photos");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/draw_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_image.save(path, "PNG");
}

void DrawBoard::drawLine(const QPoint &from, const QPoint &to)
{
    QPainter p(&m_image);
    p.setPen(QPen(QColor(0x20, 0x20, 0x20), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(from, to);
}

void DrawBoard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.drawImage(0, 0, m_image);
}

void DrawBoard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_drawing = true;
    m_last = event->pos();
    drawLine(m_last, m_last);
    update();
}

void DrawBoard::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing)
        return;
    drawLine(m_last, event->pos());
    m_last = event->pos();
    update();
}

void DrawBoard::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_drawing = false;
}

void DrawBoard::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    ensureImage();
}
