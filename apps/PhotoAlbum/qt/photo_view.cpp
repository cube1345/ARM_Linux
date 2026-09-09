#include "photo_view.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QLineF>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QTouchEvent>
#include <QWheelEvent>

/**
 * @brief 初始化图片视图及触摸交互状态。
 */
PhotoView::PhotoView(QWidget *parent)
    : QWidget(parent),
      mode(BrowseMode),
      scale(1.0),
      dragging(false),
      selecting(false),
      pinchDistance(0.0),
      pinchActive(false),
      pinchOccurred(false),
      singleTouchActive(false),
      hasLastTap(false),
      singleTouchId(-1)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
    setMouseTracking(true);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

/**
 * @brief 设置当前图片并复位视图。
 * @param image 要显示的图片。
 */
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

/**
 * @brief 将缩放、偏移和裁剪选择恢复到初始状态。
 */
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

/**
 * @brief 根据双指距离变化执行缩放。
 * @param center 双指中心点。
 * @param scaleFactor 本次缩放倍率。
 */
void PhotoView::applyPinch(const QPointF &center, qreal scaleFactor)
{
    const qreal oldScale = scale;
    const qreal candidate = qBound(1.0, scale * scaleFactor, 8.0);
    const QRectF oldRect = imageRectForScale(oldScale);
    const QRectF newRect = imageRectForScale(candidate);
    const qreal relativeX = (center.x() - oldRect.x()) / oldRect.width();
    const qreal relativeY = (center.y() - oldRect.y()) / oldRect.height();
    const QPointF newAnchor(newRect.x() + relativeX * newRect.width(),
                            newRect.y() + relativeY * newRect.height());
    offset += newAnchor - center;
    scale = candidate;
    clampOffset();
    update();
}

/**
 * @brief 处理 Qt 多点触摸事件。
 * @param event Qt 触摸事件对象。
 */
void PhotoView::handleTouchEvent(QTouchEvent *event)
{
    const QList<QTouchEvent::TouchPoint> &points = event->touchPoints();
    const bool touchDebug = qEnvironmentVariableIsSet("PHOTO_ALBUM_TOUCH_DEBUG");

    if (touchDebug) {
        qDebug() << "PhotoAlbum touch event:" << event->type()
                 << "reported points:" << points.size();
        for (int index = 0; index < points.size(); ++index) {
            const QTouchEvent::TouchPoint &point = points.at(index);
            qDebug() << " point" << point.id()
                     << "state" << point.state()
                     << "pos" << point.pos();
        }
    }

    for (int index = 0; index < points.size(); ++index) {
        const QTouchEvent::TouchPoint &point = points.at(index);
        if (point.state() == Qt::TouchPointReleased) {
            lastTouchPosition = point.pos();
            activeTouches.remove(point.id());
            touchStartPositions.remove(point.id());
        } else {
            if (!touchStartPositions.contains(point.id()))
                touchStartPositions.insert(point.id(), point.pos());
            activeTouches.insert(point.id(), point.pos());
        }
    }

    qreal maximumMovement = 0.0;
    QHash<int, QPointF>::const_iterator iterator = activeTouches.constBegin();
    while (iterator != activeTouches.constEnd()) {
        const QPointF startPosition = touchStartPositions.value(iterator.key(),
                                                               iterator.value());
        maximumMovement = qMax(maximumMovement,
                               QLineF(startPosition, iterator.value()).length());
        ++iterator;
    }
    emit touchDebugChanged(activeTouches.size(), maximumMovement >= 4.0,
                           maximumMovement);

    if (activeTouches.isEmpty()) {
        if (selecting) {
            selecting = false;
            if (selection.width() < 8 || selection.height() < 8)
                selection = QRect();
        }

        const bool quickTouch = singleTouchActive &&
                                 touchTimer.isValid() &&
                                 touchTimer.elapsed() <= 700;
        const qreal movement = QLineF(touchStartPosition,
                                      lastTouchPosition).length();

        if (!pinchOccurred && mode == BrowseMode && scale == 1.0) {
            const qreal deltaX = lastTouchPosition.x() - touchStartPosition.x();
            const qreal deltaY = lastTouchPosition.y() - touchStartPosition.y();
            if (qAbs(deltaX) >= 100 && qAbs(deltaX) > qAbs(deltaY) * 1.5) {
                if (deltaX < 0)
                    emit nextRequested();
                else
                    emit previousRequested();
            } else if (qAbs(deltaY) >= 60 && qAbs(deltaY) > qAbs(deltaX) * 1.5) {
                emit filmstripRequested(deltaY < 0);
            }
        }

        if (quickTouch && mode == BrowseMode && movement < 12.0) {
            if (hasLastTap && lastTapTimer.isValid() &&
                    lastTapTimer.elapsed() <= 450 &&
                    QLineF(lastTapPosition, lastTouchPosition).length() < 32.0) {
                resetView();
                hasLastTap = false;
            } else {
                lastTapPosition = lastTouchPosition;
                lastTapTimer.start();
                hasLastTap = true;
            }
        }

        resetTouchState();
        update();
        return;
    }

    if (activeTouches.size() >= 2) {
        const QList<QPointF> positions = activeTouches.values();
        const QPointF first = positions.at(0);
        const QPointF second = positions.at(1);
        const qreal distance = QLineF(first, second).length();
        const QPointF center((first.x() + second.x()) / 2.0,
                             (first.y() + second.y()) / 2.0);

        pinchActive = true;
        pinchOccurred = true;
        singleTouchActive = false;
        dragging = false;
        selecting = false;

        const qreal previousDistance = pinchDistance;
        if (mode == BrowseMode && pinchDistance > 0.0 && distance > 0.0)
            applyPinch(center, distance / pinchDistance);
        pinchDistance = distance;
        if (touchDebug)
            qDebug() << "PhotoAlbum pinch:" << "distance" << distance
                     << "previous" << previousDistance << "scale" << scale;
        return;
    }

    if (pinchActive || !singleTouchActive) {
        pinchActive = false;
        pinchDistance = 0.0;
        singleTouchActive = true;
        singleTouchId = activeTouches.constBegin().key();
        touchStartPosition = activeTouches.value(singleTouchId);
        lastTouchPosition = touchStartPosition;
        touchOffsetAtStart = offset;
        touchTimer.start();

        if (mode == CropMode) {
            selecting = true;
            selectionStart = touchStartPosition.toPoint();
            selection = QRect(selectionStart, QSize());
        }
    } else {
        const QPointF position = activeTouches.value(singleTouchId,
                                                     lastTouchPosition);
        lastTouchPosition = position;

        if (mode == CropMode) {
            selection = QRect(selectionStart, position.toPoint()).normalized();
        } else {
            offset = touchOffsetAtStart + position - touchStartPosition;
            clampOffset();
        }
        update();
    }
}

/**
 * @brief 清空触摸点及手势状态。
 */
void PhotoView::resetTouchState()
{
    activeTouches.clear();
    touchStartPositions.clear();
    touchStartPosition = QPointF();
    lastTouchPosition = QPointF();
    touchOffsetAtStart = QPointF();
    touchTimer.invalidate();
    pinchDistance = 0.0;
    pinchActive = false;
    pinchOccurred = false;
    singleTouchActive = false;
    singleTouchId = -1;
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
        const int thirdWidth = selected.width() / 3;
        const int thirdHeight = selected.height() / 3;
        painter.drawLine(selected.left() + thirdWidth, selected.top(),
                         selected.left() + thirdWidth, selected.bottom());
        painter.drawLine(selected.left() + thirdWidth * 2, selected.top(),
                         selected.left() + thirdWidth * 2, selected.bottom());
        painter.drawLine(selected.left(), selected.top() + thirdHeight,
                         selected.right(), selected.top() + thirdHeight);
        painter.drawLine(selected.left(), selected.top() + thirdHeight * 2,
                         selected.right(), selected.top() + thirdHeight * 2);
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

/**
 * @brief 处理鼠标或单点触摸按下事件。
 * @param event 鼠标事件对象。
 */
void PhotoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    emit touchDebugChanged(1, false, 0.0);

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

/**
 * @brief 处理拖动或裁剪框选移动事件。
 * @param event 鼠标事件对象。
 */
void PhotoView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint movementStart = selecting ? selectionStart : dragStart;
    const qreal movement = QLineF(movementStart, event->pos()).length();
    if (selecting || dragging)
        emit touchDebugChanged(1, movement >= 4.0, movement);

    if (selecting) {
        selection = QRect(selectionStart, event->pos()).normalized();
        update();
    } else if (dragging) {
        offset = dragOffset + event->pos() - dragStart;
        clampOffset();
        update();
    }
}


/**
 * @brief 处理鼠标或单点触摸释放事件。
 * @param event 鼠标事件对象。
 */
void PhotoView::mouseReleaseEvent(QMouseEvent *event)
{
    emit touchDebugChanged(0, false, 0.0);

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
        const int deltaY = event->y() - dragStart.y();
        if (scale == 1.0) {
            if (qAbs(deltaX) >= 100 && qAbs(deltaX) > qAbs(deltaY) * 1.5) {
                if (deltaX < 0)
                    emit nextRequested();
                else
                    emit previousRequested();
            } else if (qAbs(deltaY) >= 60 && qAbs(deltaY) > qAbs(deltaX) * 1.5) {
                emit filmstripRequested(deltaY < 0);
            }
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

/**
 * @brief 分发 Qt 触摸事件，其余事件交给 QWidget。
 * @param event Qt 通用事件对象。
 * @return 事件是否已处理。
 */
bool PhotoView::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        handleTouchEvent(static_cast<QTouchEvent *>(event));
        event->accept();
        return true;
    default:
        break;
    }

    return QWidget::event(event);
}
