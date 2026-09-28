#ifndef PHOTO_VIEW_H
#define PHOTO_VIEW_H

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRubberBand>
#include <QWidget>

class QTouchEvent;
/**
 * @brief 显示图片并处理浏览、拖动、双指缩放和裁剪。
 *
 * 控件同时接收 Qt 触摸事件和鼠标事件。鼠标事件用于兼容 evdevtouch
 * 将触摸输入转换为单指指针事件的平台。
 */
class PhotoView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 图片视图的交互模式。
     */
    enum ViewMode {
        BrowseMode,
        CropMode
    };

    /**
     * @brief 创建空的图片视图。
     * @param parent Qt 父控件，可选。
     */
    explicit PhotoView(QWidget *parent = nullptr);

    /** @brief 设置显示图片并复位视图。 */
    void setImage(const QImage &image);
    /** @brief 返回当前图片。 */
    QImage image() const;
    /** @brief 返回当前交互模式。 */
    ViewMode viewMode() const;
    /** @brief 在浏览模式和裁剪模式之间切换。 */
    void setViewMode(ViewMode mode);
    /** @brief 复位缩放、平移偏移量和裁剪区域。 */
    void resetView();
    /** @brief 返回当前是否存在有效裁剪矩形。 */
    bool hasCropSelection() const;
    /** @brief 返回当前裁剪矩形对应的图片。 */
    QImage selectedImage() const;
    /** @brief 返回当前图片尺寸。 */
    QSize imageSize() const;
    /** @brief 设置屏幕旋转角度（0/90/180/270），用于手势方向映射。 */
    void setRotationAngle(int angle) { m_rotationAngle = angle; }

signals:
    /** @brief 请求显示上一张图片。 */
    void previousRequested();
    /** @brief 请求显示下一张图片。 */
    void nextRequested();
    /**
     * @brief 单指垂直滑动。upward=true 上滑（展示拍摄信息），false 下滑（展示操作栏）。
     */
    void verticalSwipeRequested(bool upward);
    /**
     * @brief 向主窗口报告触摸调试信息。
     * @param contactCount Number of active contacts.
     * @param sliding 是否有接触点移动达到判定阈值。
     * @param movement 接触点最大移动距离，单位为像素。
     */
    void touchDebugChanged(int contactCount, bool sliding, qreal movement);

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
    void transformDelta(qreal &dx, qreal &dy) const;

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
    QHash<int, QPointF> touchStartPositions;
    QPointF touchStartPosition;
    QPointF lastTouchPosition;
    QPointF lastTapPosition;
    QPointF touchOffsetAtStart;
    QElapsedTimer touchTimer;
    QElapsedTimer lastTapTimer;
    qreal pinchDistance;
    QPointF pinchCenter;
    int m_rotationAngle;
    bool pinchActive;
    bool pinchOccurred;
    bool singleTouchActive;
    bool hasLastTap;
    int singleTouchId;
};
#endif
