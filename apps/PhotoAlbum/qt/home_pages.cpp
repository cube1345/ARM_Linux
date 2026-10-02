#include "home_pages.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace {
const int kSwipeThreshold = 42;   // px：超过即判定为横向滑动切页
const int kDotRadius = 5;         // 圆点半宽
}

HomePageView::HomePageView(QWidget *parent)
    : QWidget(parent)
    , m_current(0)
    , m_startX(0)
    , m_tracking(false)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    m_pages = new QStackedWidget(this);
    m_pages->setStyleSheet(QStringLiteral("background:transparent;"));
    m_pages->installEventFilter(this);
    lay->addWidget(m_pages, 1);

    m_dotBar = new QWidget(this);
    QHBoxLayout *dotLay = new QHBoxLayout(m_dotBar);
    dotLay->setContentsMargins(0, 0, 0, 0);
    dotLay->setSpacing(10);
    dotLay->addStretch();
    // 圆点由 addPage() 动态添加
    m_dotBar->setLayout(dotLay);
    lay->addWidget(m_dotBar);
}

void HomePageView::addPage(QWidget *page)
{
    m_pages->addWidget(page);

    QLabel *dot = new QLabel(m_dotBar);
    dot->setFixedSize(2 * kDotRadius, 2 * kDotRadius);
    dot->setStyleSheet(QStringLiteral("border-radius:%1px;background:rgba(255,255,255,0.35);")
                           .arg(kDotRadius));
    static_cast<QHBoxLayout *>(m_dotBar->layout())->insertWidget(
        static_cast<QHBoxLayout *>(m_dotBar->layout())->count() - 1, dot);
    m_dots.append(dot);
    updateDots();
}

void HomePageView::setPage(int index)
{
    if (index < 0 || index >= m_pages->count() || index == m_current)
        return;
    m_pages->setCurrentIndex(index);
    m_current = index;
    updateDots();
}

void HomePageView::endSwipe(int deltaX)
{
    m_tracking = false;
    if (qAbs(deltaX) < kSwipeThreshold)
        return;                     // 未达阈值：视为点击，交给按钮
    setPage(m_current + (deltaX < 0 ? 1 : -1));
}

void HomePageView::updateDots()
{
    for (int i = 0; i < m_dots.size(); ++i) {
        m_dots[i]->setStyleSheet(
            QStringLiteral("border-radius:%1px;background:%2;")
                .arg(kDotRadius)
                .arg(i == m_current ? QStringLiteral("#ffffff")
                                    : QStringLiteral("rgba(255,255,255,0.35)")));
    }
}

bool HomePageView::eventFilter(QObject *obj, QEvent *ev)
{
    switch (ev->type()) {
    case QEvent::MouseButtonPress: {
        const QMouseEvent *me = static_cast<QMouseEvent *>(ev);
        if (me->button() == Qt::LeftButton) {
            m_startX = int(me->pos().x());
            m_tracking = true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (m_tracking) {
            const QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            const int dx = int(me->pos().x()) - m_startX;
            // 滑动切页时吞掉事件，避免误触按钮
            if (qAbs(dx) >= kSwipeThreshold) {
                endSwipe(dx);
                return true;
            }
            m_tracking = false;
        }
        break;
    }
    case QEvent::MouseMove: {
        if (m_tracking) {
            const QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            if (qAbs(int(me->pos().x()) - m_startX) >= kSwipeThreshold) {
                endSwipe(int(me->pos().x()) - m_startX);
                return true;
            }
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(obj, ev);
}