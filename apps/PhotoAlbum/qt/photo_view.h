#ifndef PHOTO_VIEW_H
#define PHOTO_VIEW_H

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRubberBand>
#include <QWidget>

class QTouchEvent;
class PhotoView : public QWidget
{
    Q_OBJECT

public:
    enum ViewMode {
        BrowseMode,
        CropMode
    };

    explicit PhotoView(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    QImage image() const;
    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);
    void resetView();
    bool hasCropSelection() const;
    QImage selectedImage() const;
    QSize imageSize() const;

signals:
    void previousRequested();
    void nextRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;

private:
    QRectF imageRect() const;
    QRectF imageRectForScale(qreal scale) const;
    void clampOffset();
    void applyPinch(const QPointF &center, qreal scaleFactor);
    void handleTouchEvent(QTouchEvent *event);
    void resetTouchState();
    void paintCropOverlay(QPainter &painter);
    QPoint mapToImage(const QPoint &point) const;

    QImage currentImage;
    ViewMode mode;
    qreal scale;
    QPointF offset;
    bool dragging;
    QPoint dragStart;
    QPointF dragOffset;
    bool selecting;
    QPoint selectionStart;
    QRect selection;
    QHash<int, QPointF> activeTouches;
    QPointF touchStartPosition;
    QPointF lastTouchPosition;
    QPointF lastTapPosition;
    QPointF touchOffsetAtStart;
    QElapsedTimer touchTimer;
    QElapsedTimer lastTapTimer;
    qreal pinchDistance;
    bool pinchActive;
    bool pinchOccurred;
    bool singleTouchActive;
    bool hasLastTap;
    int singleTouchId;
};
#endif
