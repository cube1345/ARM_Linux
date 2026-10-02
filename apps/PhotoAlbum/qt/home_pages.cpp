#include "home_pages.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace {
const int kSwipeThreshold = 42;   // px：超过即判定为横向滑动切页
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
}

void HomePageView::addPage(QWidget *page)
{
    m_pages->addWidget(page);

    // 关键：Qt 的子部件事件不会自动冒泡到父级 eventFilter，
    // 递归给页面内所有子部件装 filter，滑动检测才能收到 press/move/release
    installSwipeFilter(page);
}

void HomePageView::installSwipeFilter(QWidget *w)
{
    w->installEventFilter(this);
    const auto children = w->children();
    for (QObject *c : children) {
        if (QWidget *cw = qobject_cast<QWidget *>(c))
            installSwipeFilter(cw);
    }
}

void HomePageView::setPage(int index)
{
    if (index < 0 || index >= m_pages->count() || index == m_current)
        return;
    m_pages->setCurrentIndex(index);
    m_current = index;
}

void HomePageView::endSwipe(int deltaX)
{
    m_tracking = false;
    if (qAbs(deltaX) < kSwipeThreshold)
        return;                     // 未达阈值：视为点击，交给按钮
    setPage(m_current + (deltaX < 0 ? 1 : -1));
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
    case QEvent::TouchBegin: {
        const QTouchEvent *te = static_cast<QTouchEvent *>(ev);
        const auto pts = te->touchPoints();
        if (!pts.isEmpty()) {
            m_startX = int(pts.first().pos().x());
            m_tracking = true;
        }
        break;
    }
    case QEvent::TouchEnd: {
        if (m_tracking) {
            const QTouchEvent *te = static_cast<QTouchEvent *>(ev);
            const auto pts = te->touchPoints();
            if (!pts.isEmpty()) {
                const int dx = int(pts.last().pos().x()) - m_startX;
                if (qAbs(dx) >= kSwipeThreshold) {
                    endSwipe(dx);
                    return true;
                }
            }
            m_tracking = false;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(obj, ev);
}