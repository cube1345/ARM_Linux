#include "photo_view.h"

#include <QDate>
#include <QDateTime>
#include <QGestureEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPinchGesture>
#include <QWheelEvent>

PhotoView::PhotoView(QWidget *parent)
    : QWidget(parent),
      mode(BrowseMode),
      scale(1.0),
      dragging(false),
      selecting(false)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
    grabGesture(Qt::PinchGesture);
    setMouseTracking(true);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PhotoView::setImage(const QImage &image)
{
    currentImage = image;
    resetView();
    update();
}

QImage PhotoView::image() const
{
    return currentImage;
}

PhotoView::ViewMode PhotoView::viewMode() const
{
    return mode;
}

void PhotoView::setViewMode(ViewMode newMode)
{
    mode = newMode;
    selection = QRect();
    setCursor(mode == CropMode ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void PhotoView::resetView()
{
    scale = 1.0;
    offset = QPointF(0, 0);
    selection = QRect();
    update();
}

bool PhotoView::hasCropSelection() const
{
    return mode == CropMode && selection.width() >= 8 && selection.height() >= 8;
}

QImage PhotoView::selectedImage() const
{
    if (!hasCropSelection())
        return QImage();

    const QRectF drawn = imageRect();
    const QRect normalized = selection.normalized().intersected(drawn.toRect());
    const qreal xRatio = currentImage.width() / drawn.width();
    const qreal yRatio = currentImage.height() / drawn.height();
    const int x = qBound(0, qRound((normalized.x() - drawn.x()) * xRatio),
                         currentImage.width() - 1);
    const int y = qBound(0, qRound((normalized.y() - drawn.y()) * yRatio),
                         currentImage.height() - 1);
    const int right = qBound(x + 1, qRound((normalized.right() - drawn.x()) * xRatio),
                             currentImage.width());
    const int bottom = qBound(y + 1, qRound((normalized.bottom() - drawn.y()) * yRatio),
                              currentImage.height());
    return currentImage.copy(x, y, right - x, bottom - y);
}

QSize PhotoView::imageSize() const
{
    return currentImage.size();
}

void PhotoView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0));

    if (currentImage.isNull()) {
        painter.setPen(QColor(150, 150, 155));
        painter.setFont(font());
        painter.drawText(rect(), Qt::AlignCenter, tr("No photo"));
        return;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(imageRect().toRect(), currentImage);

    if (mode == CropMode)
        paintCropOverlay(painter);
}

QRectF PhotoView::imageRect() const
{
    return imageRectForScale(scale);
}

QRectF PhotoView::imageRectForScale(qreal requestedScale) const
{
    if (currentImage.isNull())
        return QRectF();

    const qreal fitScale = qMin(static_cast<qreal>(width()) / currentImage.width(),
                                static_cast<qreal>(height()) / currentImage.height());
    const qreal effectiveScale = fitScale * requestedScale;
    const qreal width = currentImage.width() * effectiveScale;
    const qreal height = currentImage.height() * effectiveScale;
    return QRectF((this->width() - width) / 2.0 + offset.x(),
                  (this->height() - height) / 2.0 + offset.y(), width, height);
}

void PhotoView::clampOffset()
{
    if (currentImage.isNull())
        return;

    const QRectF drawn = imageRect();
    const qreal maximumX = qMax<qreal>(0, drawn.width() - width()) / 2.0;
    const qreal maximumY = qMax<qreal>(0, drawn.height() - height()) / 2.0;
    offset.setX(qBound(-maximumX, offset.x(), maximumX));
    offset.setY(qBound(-maximumY, offset.y(), maximumY));
}

void PhotoView::applyPinch(const QPinchGesture *gesture)
{
    const QPointF localCenter = mapFromGlobal(gesture->centerPoint().toPoint());
    const qreal oldScale = scale;
    const qreal candidate = qBound(1.0, scale * gesture->scaleFactor(), 8.0);
    const QRectF oldRect = imageRectForScale(oldScale);
    const QRectF newRect = imageRectForScale(candidate);
    const qreal relativeX = (localCenter.x() - oldRect.x()) / oldRect.width();
    const qreal relativeY = (localCenter.y() - oldRect.y()) / oldRect.height();
    const QPointF newAnchor(newRect.x() + relativeX * newRect.width(),
                            newRect.y() + relativeY * newRect.height());
    offset += newAnchor - localCenter;
    scale = candidate;
    clampOffset();
    update();
}

void PhotoView::paintCropOverlay(QPainter &painter)
{
    painter.save();
    QColor shade(0, 0, 0, 150);
    if (!selection.isNull()) {
        const QRect selected = selection.normalized();
        QRegion outside(rect());
        outside = outside.subtracted(QRegion(selected));
        painter.setClipRegion(outside);
        painter.fillRect(rect(), shade);
        painter.setClipping(false);
        painter.setPen(QPen(QColor(255, 255, 255, 220), 2));
        painter.drawRect(selected);
        painter.setPen(QPen(QColor(255, 255, 255, 140), 1));
        painter.drawRect(selected.adjusted(0, selected.height() / 3,
                                           0, selected.height() / 3));
    } else {
        painter.fillRect(rect(), QColor(0, 0, 0, 35));
        painter.setPen(QColor(255, 255, 255, 170));
        painter.drawText(rect(), Qt::AlignCenter, tr("Drag to select crop area"));
    }
    painter.restore();
}

QPoint PhotoView::mapToImage(const QPoint &point) const
{
    return point;
}

void PhotoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (mode == CropMode) {
        selecting = true;
        selectionStart = event->pos();
        selection = QRect(selectionStart, QSize());
    } else {
        dragging = true;
        dragStart = event->pos();
        dragOffset = offset;
    }
    update();
}

void PhotoView::mouseMoveEvent(QMouseEvent *event)
{
    if (selecting) {
        selection = QRect(selectionStart, event->pos()).normalized();
        update();
    } else if (dragging) {
        offset = dragOffset + event->pos() - dragStart;
        clampOffset();
        update();
    }
}

void PhotoView::mouseReleaseEvent(QMouseEvent *event)
{
    if (selecting) {
        selecting = false;
        selection = QRect(selectionStart, event->pos()).normalized();
        if (selection.width() < 8 || selection.height() < 8)
            selection = QRect();
        update();
        return;
    }

    if (dragging) {
        dragging = false;
        const int deltaX = event->x() - dragStart.x();
        if (scale == 1.0 && qAbs(deltaX) >= 100) {
            if (deltaX < 0)
                emit nextRequested();
            else
                emit previousRequested();
        }
    }
}

void PhotoView::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (mode == BrowseMode)
        resetView();
}

void PhotoView::wheelEvent(QWheelEvent *event)
{
    if (mode != BrowseMode)
        return;

    const int steps = event->angleDelta().y() / 120;
    if (steps != 0)
        scale = qBound(1.0, scale + steps * 0.2, 8.0);
    clampOffset();
    update();
}

bool PhotoView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        QGestureEvent *gestures = static_cast<QGestureEvent *>(event);
        if (QPinchGesture *pinch = qobject_cast<QPinchGesture *>(
                gestures->gesture(Qt::PinchGesture))) {
            if (mode == BrowseMode)
                applyPinch(pinch);
            return true;
        }
    }
    return QWidget::event(event);
}
