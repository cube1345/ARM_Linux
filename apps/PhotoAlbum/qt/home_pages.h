#ifndef HOME_PAGES_H
#define HOME_PAGES_H

#include <QWidget>
#include <QStackedWidget>
#include <QVector>
#include <QLabel>

// 桌面分页容器：多页 + 左右滑动切换 + 底部圆点指示器
// 触摸/鼠标横向滑动过阈值即切页；点击不干扰
class HomePageView : public QWidget {
    Q_OBJECT
public:
    explicit HomePageView(QWidget *parent = nullptr);

    void addPage(QWidget *page);              // 追加一页到末尾
    int pageCount() const { return m_pages->count(); }
    void setPage(int index);                  // 直接切到指定页（不带动画）

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void endSwipe(int deltaX);                // 滑动结束：按方向切页
    void updateDots();

    QStackedWidget *m_pages;
    QWidget *m_dotBar;
    QVector<QLabel *> m_dots;
    int m_current;
    int m_startX;
    bool m_tracking;                          // 手指已按下
};

#endif // HOME_PAGES_H