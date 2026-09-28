#ifndef DRAWBOARD_H
#define DRAWBOARD_H

#include <QImage>
#include <QPoint>
#include <QWidget>

class DrawBoard : public QWidget
{
    Q_OBJECT
public:
    explicit DrawBoard(QWidget *parent = nullptr);

    void clear();
    void save();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QImage m_image;
    QPoint m_last;
    bool m_drawing;

    void drawLine(const QPoint &from, const QPoint &to);
    void ensureImage();
};

#endif
