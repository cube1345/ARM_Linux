#include "main_window.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QTimer>
#include <functional>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextStream>
#include <QDebug>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImageReader>
#include <QtMath>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QToolButton>
#include <QSlider>
#include <QTime>
#include <QVariant>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "photo_view.h"
#include "video_player.h"
#include "icm20608.h"
#include "tank_game.h"
#include "snake_game.h"
#include "tetris_game.h"
#include "brick_game.h"
#include "game2048.h"
#include "music_player.h"
#include "weather.h"
#include "calculator.h"
#include "drawboard.h"
#include <QColor>
#include "home_pages.h"
#include "icon_factory.h"
#include "debug_page.h"
#include "debug_overlay.h"
#include "config.h"
#include <unistd.h>

/**
 * @brief 让缩略图区域支持按住拖动滚动：从图片按钮上起手也能滚动，
 *        轻点仍正常触发按钮点击。
 */
class ThumbDragScroll : public QObject
{
public:
    explicit ThumbDragScroll(QScrollArea *area, QObject *parent = nullptr)
        : QObject(parent), area(area), pressing(false), active(false), m_rotationAngle(0)
    {
        area->viewport()->installEventFilter(this);
    }

    void setRotationAngle(int angle) { m_rotationAngle = angle; }

    void attach(QWidget *widget)
    {
        widget->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() != Qt::LeftButton)
                return false;
            pressing = true;
            active = false;
            lastGlobal = mouseEvent->globalPos();
            return false;
        }
        case QEvent::MouseMove: {
            if (!pressing)
                return false;
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (!active) {
                if ((mouseEvent->globalPos() - lastGlobal).manhattanLength() < 12)
                    return false;
                active = true;
                QAbstractButton *button = qobject_cast<QAbstractButton *>(obj);
                if (button)
                    button->setDown(false);
            }
            const QPoint delta = mouseEvent->globalPos() - lastGlobal;
            lastGlobal = mouseEvent->globalPos();
            qreal dx = delta.x();
            qreal dy = delta.y();
            switch (m_rotationAngle) {
            case 90: qSwap(dx, dy); dy = -dy; break;
            case 180: dx = -dx; dy = -dy; break;
            case 270: qSwap(dx, dy); dx = -dx; break;
            default: break;
            }
            if (QScrollBar *verticalBar = area->verticalScrollBar())
                if (verticalBar->isVisible() && verticalBar->maximum() > 0)
                    verticalBar->setValue(verticalBar->value() - dy);
            if (QScrollBar *horizontalBar = area->horizontalScrollBar())
                if (horizontalBar->isVisible() && horizontalBar->maximum() > 0)
                    horizontalBar->setValue(horizontalBar->value() - dx);
            return true;
        }
        case QEvent::MouseButtonRelease: {
            if (!pressing)
                return false;
            pressing = false;
            if (active) {
                active = false;
                return true;
            }
            return false;
        }
        default:
            return false;
        }
    }

private:
    QScrollArea *area;
    int m_rotationAngle;
    bool pressing;
    bool active;
    QPoint lastGlobal;
};

class ThumbnailButton : public QAbstractButton
{
public:
    explicit ThumbnailButton(QWidget *parent = nullptr) : QAbstractButton(parent) {}
    void setThumbnail(const QImage &image)
    {
        m_image = image;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        if (m_image.isNull()) {
            painter.fillRect(rect(), QColor(0x14, 0x14, 0x1a));
            return;
        }
        const QSize isz = m_image.size();
        if (isz.width() <= 0 || isz.height() <= 0)
            return;
        const QRectF target(rect());
        const qreal scale = qMax(target.width() / isz.width(),
                                 target.height() / isz.height());
        const qreal cw = target.width() / scale;
        const qreal ch = target.height() / scale;
        const QRectF source((isz.width() - cw) / 2.0,
                            (isz.height() - ch) / 2.0,
                            cw, ch);
        painter.drawImage(target, m_image, source);
    }
private:
    QImage m_image;
};

/**
 * @brief 初始化相册主窗口。
 */
MainWindow::MainWindow(const QString &photoDirectory, QWidget *parent)
    : QMainWindow(parent),
      stackedWidget(nullptr),
      homePage(nullptr),
      monitorPage(nullptr),
      thumbnailPage(nullptr),
      detailPage(nullptr),
      videoListPage(nullptr),
      videoPage(nullptr),
      videoReturnPage(nullptr),
      gamePage(nullptr),
      levelSelectPage(nullptr),
      tankGame(nullptr),
      snakePage(nullptr),
      tetrisPage(nullptr),
      brickPage(nullptr),
      game2048Page(nullptr),
      musicPage(nullptr),
      settingsPage(nullptr),
      debugPage(nullptr),
      debugWidget(nullptr),
      m_debugOverlay(nullptr),
      snakeGame(nullptr),
      tetrisGame(nullptr),
      brickGame(nullptr),
      game2048(nullptr),
      musicPlayer(nullptr),
      calculatorPage(nullptr),
      drawPage(nullptr),
      calculator(nullptr),
      drawBoard(nullptr),
      weather(nullptr),
      weatherLabel(nullptr),
      alsLabel(nullptr),
      monitorRefreshTimer(nullptr),
      footerBar(nullptr),
      infoPanel(nullptr),
      gridDragScroll(nullptr),
      thumbnailGrid(nullptr),
      photoView(nullptr),
      videoList(nullptr),
      videoPlayer(nullptr),
      videoTitle(nullptr),
      videoPlayButton(nullptr),
      videoSeekSlider(nullptr),
      videoTimeLabel(nullptr),
      videoDeleteButton(nullptr),
      networkManager(nullptr),
      sliderDragging(false),
      backButton(nullptr),
      titleLabel(nullptr),
      statusLabel(nullptr),
      touchCountLabel(nullptr),
      touchSlideLabel(nullptr),
      cropButton(nullptr),
      deleteButton(nullptr),
      editButton(nullptr),
      applyButton(nullptr),
      cancelButton(nullptr),
      resetButton(nullptr),
      saveButton(nullptr),
      loadWatcher(nullptr),
      loadingPath(QString()),
      loadGeneration(0),
      m_imuTimer(nullptr),
      m_orientation(-1),
      m_orientationLocked(false),
      m_view(nullptr),
      m_scene(nullptr),
      m_proxy(nullptr),
      currentIndex(0)
{
    buildUi();
    installGlobalGestures();
    loadPhotos(photoDirectory);

    m_imuTimer = new QTimer(this);
    if (m_imu.open()) {
        m_imu.init();
        connect(m_imuTimer, &QTimer::timeout, this, &MainWindow::onImuTick);
        m_imuTimer->start(200);
    } else {
        qWarning() << "[IMU] spidev 不可用，方向锁定功能禁用";
    }

    weather = new Weather(this);
    connect(weather, &Weather::ready, this, [this](const QString &temp, const QString &desc, const QString &) {
        if (weatherLabel)
            weatherLabel->setText(QStringLiteral("%1 %2°C").arg(desc).arg(temp));
    });
    weather->fetch();
    QTimer *weatherTimer = new QTimer(this);
    weatherTimer->setInterval(1800000);
    connect(weatherTimer, &QTimer::timeout, weather, &Weather::fetch);
    weatherTimer->start();

    if (m_als.open()) {
        m_als.init();
        QTimer *alsTimer = new QTimer(this);
        alsTimer->setInterval(500);
        connect(alsTimer, &QTimer::timeout, this, [this]() {
            if (alsLabel)
                alsLabel->setText(QStringLiteral("光照 %1").arg(m_als.readAls()));
        });
        alsTimer->start();
    } else if (alsLabel) {
        alsLabel->setText(tr("光照不可用"));
    }

    m_debugOverlay = new DebugOverlay(this);
    m_debugOverlay->setVisible(qEnvironmentVariableIsSet("PHOTO_ALBUM_DEBUG"));
    m_debugOverlay->move(10, 10);
    m_debugOverlay->raise();
}

static int detectOrientation(const QVector3D &a)
{
    const float ax = a.x();
    const float ay = a.y();
    const float az = a.z();
    const float absx = qAbs(ax);
    const float absy = qAbs(ay);
    const float absz = qAbs(az);
    if (absz > absx && absz > absy)
        return -1;
    if (absx > absy)
        return ax > 0 ? 0 : 2;
    return ay > 0 ? 1 : 3;
}

void MainWindow::onImuTick()
{
    if (!m_imu.isOpen())
        return;
    const QVector3D a = m_imu.readAccel();
    const int orient = detectOrientation(a);
    if (orient != m_orientation) {
        m_orientation = orient;
        qInfo().noquote() << QStringLiteral("[IMU] orientation=%1 accel=%2 %3 %4")
            .arg(orient)
            .arg(a.x(), 0, 'f', 2).arg(a.y(), 0, 'f', 2).arg(a.z(), 0, 'f', 2);
        if (!m_orientationLocked)
            applyOrientation(orient);
    }

    const qreal mag = qSqrt(qreal(a.x() * a.x() + a.y() * a.y() + a.z() * a.z()));
    if (mag > 1.8 || mag < 0.4) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastShake > 1200) {
            m_lastShake = now;
            if (static_cast<QStackedWidget *>(stackedWidget)->currentWidget() == detailPage)
                showNext();
        }
    }
}

void MainWindow::applyOrientation(int orient)
{
    if (!m_view || !m_proxy || orient < 0)
        return;
    int angle = 0;
    QSize size(1024, 600);
    switch (orient) {
    case 0: angle = 90; size = QSize(600, 1024); break;
    case 1: angle = 0; break;
    case 2: angle = 270; size = QSize(600, 1024); break;
    case 3: angle = 180; break;
    }
    m_view->resetTransform();
    if (angle != 0)
        m_view->rotate(angle);
    if (photoView)
        photoView->setRotationAngle(angle);
    if (gridDragScroll)
        gridDragScroll->setRotationAngle(angle);
    m_proxy->resize(size);
    m_view->setSceneRect(m_proxy->boundingRect());
    if (static_cast<QStackedWidget *>(stackedWidget)->currentWidget() == thumbnailPage)
        rebuildThumbnailGrid();
}

/**
 * @brief 创建窗口布局、控件及信号槽连接。
 */
/**
 * @brief 创建缩略图页、详情页和固定底部操作栏。
 */
// ---- 全局单指滑动手势：任意位置滑动即可驱动当前小游戏的方向 ----
void MainWindow::installGlobalGestures()
{
    QWidget *root = static_cast<QStackedWidget *>(stackedWidget);
    std::function<void(QWidget *)> rec = [this, &rec](QWidget *w) {
        w->installEventFilter(this);
        const auto kids = w->children();
        for (QObject *c : kids)
            if (QWidget *cw = qobject_cast<QWidget *>(c))
                rec(cw);
    };
    rec(root);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    (void)obj;
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            m_gestureX = me->pos().x();
            m_gestureY = me->pos().y();
            m_gestureTracking = true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (m_gestureTracking) {
            m_gestureTracking = false;
            const QMouseEvent *me = static_cast<QMouseEvent *>(event);
            const int dx = me->pos().x() - m_gestureX;
            const int dy = me->pos().y() - m_gestureY;
            if (qMax(qAbs(dx), qAbs(dy)) >= 42) {
                handleGameSwipe(dx, dy);
                return true;             // 吞掉滑动，避免误触方向按钮
            }
        }
        break;
    }
    case QEvent::TouchBegin: {
        const QTouchEvent *te = static_cast<QTouchEvent *>(event);
        const auto pts = te->touchPoints();
        if (!pts.isEmpty()) {
            m_gestureX = int(pts.first().pos().x());
            m_gestureY = int(pts.first().pos().y());
            m_gestureTracking = true;
        }
        break;
    }
    case QEvent::TouchEnd: {
        if (m_gestureTracking) {
            m_gestureTracking = false;
            const QTouchEvent *te = static_cast<QTouchEvent *>(event);
            const auto pts = te->touchPoints();
            if (!pts.isEmpty()) {
                const int dx = int(pts.last().pos().x()) - m_gestureX;
                const int dy = int(pts.last().pos().y()) - m_gestureY;
                if (qMax(qAbs(dx), qAbs(dy)) >= 42) {
                    handleGameSwipe(dx, dy);
                    return true;
                }
            }
        }
        break;
    }
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::handleGameSwipe(int dx, int dy)
{
    QWidget *cur = static_cast<QStackedWidget *>(stackedWidget)->currentWidget();
    if (cur == game2048Page) {
        if (qAbs(dx) > qAbs(dy))
            game2048->move(dx > 0 ? Game2048::DirRight : Game2048::DirLeft);
        else
            game2048->move(dy > 0 ? Game2048::DirDown : Game2048::DirUp);
    } else if (cur == snakePage) {
        if (qAbs(dx) > qAbs(dy))
            snakeGame->setDirection(dx > 0 ? SnakeGame::DirRight : SnakeGame::DirLeft);
        else
            snakeGame->setDirection(dy > 0 ? SnakeGame::DirDown : SnakeGame::DirUp);
    } else if (cur == tetrisPage) {
        if (qAbs(dx) > qAbs(dy)) {
            if (dx > 0)
                tetrisGame->moveRight();
            else
                tetrisGame->moveLeft();
        } else {
            if (dy < 0)
                tetrisGame->rotate();
            else
                tetrisGame->softDrop();
        }
    } else if (cur == brickPage) {
        if (qAbs(dx) > qAbs(dy)) {
            brickGame->setPaddleDir(dx > 0 ? 1 : -1);
            QTimer::singleShot(320, this, [this]() { brickGame->setPaddleDir(0); });
        }
    }
    // 桌面/相册/监控/视频等页面各有自己的手势，此处不吞
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Photo Album"));
    setMinimumSize(480, 272);

    QWidget *central = new QWidget();
    central->setObjectName(QStringLiteral("root"));
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    QStackedWidget *stack = new QStackedWidget(central);
    stackedWidget = stack;
    homePage = new QWidget(stack);
    monitorPage = new QWidget(stack);
    thumbnailPage = new QWidget(stack);
    detailPage = new QWidget(stack);
    videoListPage = new QWidget(stack);
    videoPage = new QWidget(stack);
    gamePage = new QWidget(stack);
    levelSelectPage = new QWidget(stack);
    snakePage = new QWidget(stack);
    tetrisPage = new QWidget(stack);
    brickPage = new QWidget(stack);
    game2048Page = new QWidget(stack);
    musicPage = new QWidget(stack);
    calculatorPage = new QWidget(stack);
    drawPage = new QWidget(stack);
    settingsPage = new QWidget(stack);
    debugPage = new QWidget(stack);
    stack->addWidget(homePage);
    stack->addWidget(monitorPage);
    stack->addWidget(thumbnailPage);
    stack->addWidget(detailPage);
    stack->addWidget(videoListPage);
    stack->addWidget(videoPage);
    stack->addWidget(gamePage);
    stack->addWidget(levelSelectPage);
    stack->addWidget(snakePage);
    stack->addWidget(tetrisPage);
    stack->addWidget(brickPage);
    stack->addWidget(game2048Page);
    stack->addWidget(musicPage);
    stack->addWidget(calculatorPage);
    stack->addWidget(drawPage);
    stack->addWidget(settingsPage);
    stack->addWidget(debugPage);
    layout->addWidget(stack, 1);

    buildHomePage(homePage);
    buildMonitorPage(monitorPage);
    buildThumbnailPage(thumbnailPage);
    buildVideoListPage(videoListPage);
    buildVideoPage(videoPage);
    buildGamePage(gamePage);
    buildLevelSelectPage(levelSelectPage);
    buildSnakePage(snakePage);
    buildTetrisPage(tetrisPage);
    buildBrickPage(brickPage);
    buildGame2048Page(game2048Page);
    buildMusicPage(musicPage);
    buildCalculatorPage(calculatorPage);
    buildDrawPage(drawPage);
    buildSettingsPage(settingsPage);
    buildDebugPage(debugPage);

    QVBoxLayout *detailLayout = new QVBoxLayout(detailPage);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(3);
    QWidget *header = new QWidget(detailPage);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    backButton = new QPushButton(tr("返回"), header);
    backButton->setMinimumSize(48, 32);
    titleLabel = new QLabel(tr("相册"), header);
    statusLabel = new QLabel(header);
    touchCountLabel = new QLabel(tr("接触点:0"), header);
    touchSlideLabel = new QLabel(tr("滑动:否"), header);
    titleLabel->setObjectName(QStringLiteral("title"));
    statusLabel->setObjectName(QStringLiteral("status"));
    touchCountLabel->setObjectName(QStringLiteral("touchCount"));
    touchSlideLabel->setObjectName(QStringLiteral("touchSlide"));
    const bool touchDebug = qEnvironmentVariableIsSet("PHOTO_ALBUM_TOUCH_DEBUG");
    touchCountLabel->setVisible(touchDebug);
    touchSlideLabel->setVisible(touchDebug);
    headerLayout->addWidget(backButton);
    headerLayout->addStretch();
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusLabel);
    headerLayout->addWidget(touchCountLabel);
    headerLayout->addWidget(touchSlideLabel);
    detailLayout->addWidget(header);

    infoPanel = new QLabel(detailPage);
    infoPanel->setObjectName(QStringLiteral("infoPanel"));
    infoPanel->setWordWrap(true);
    infoPanel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoPanel->setVisible(false);
    detailLayout->addWidget(infoPanel);

    photoView = new PhotoView(detailPage);
    connect(photoView, SIGNAL(previousRequested()), this, SLOT(showPrevious()));
    connect(photoView, SIGNAL(nextRequested()), this, SLOT(showNext()));
    connect(photoView, SIGNAL(verticalSwipeRequested(bool)),
            this, SLOT(onVerticalSwipe(bool)));
    connect(photoView, SIGNAL(touchDebugChanged(int,bool,qreal)),
            this, SLOT(updateTouchDebug(int,bool,qreal)));
    detailLayout->addWidget(photoView, 1);

    footerBar = new QWidget(detailPage);
    footerBar->setObjectName(QStringLiteral("footerBar"));
    footerBar->setMinimumHeight(0);
    footerBar->setMaximumHeight(45);
    footerBar->setVisible(false);
    QHBoxLayout *footerLayout = new QHBoxLayout(footerBar);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(5);
    cropButton = new QPushButton(tr("裁剪"), footerBar);
    deleteButton = new QPushButton(tr("删除"), footerBar);
    saveButton = new QPushButton(tr("另存为"), footerBar);
    editButton = new QPushButton(tr("编辑"), footerBar);
    editButton->setEnabled(false);
    applyButton = new QPushButton(tr("应用"), footerBar);
    cancelButton = new QPushButton(tr("取消"), footerBar);
    resetButton = new QPushButton(tr("复位"), footerBar);
    cropButton->setMinimumSize(48, 32);
    deleteButton->setMinimumSize(48, 32);
    saveButton->setMinimumSize(48, 32);
    editButton->setMinimumSize(48, 32);
    applyButton->setMinimumSize(48, 32);
    cancelButton->setMinimumSize(48, 32);
    resetButton->setMinimumSize(48, 32);
    cropButton->setObjectName(QStringLiteral("primaryButton"));
    deleteButton->setObjectName(QStringLiteral("dangerButton"));
    applyButton->setObjectName(QStringLiteral("primaryButton"));
    footerLayout->addStretch(); footerLayout->addWidget(cropButton);
    footerLayout->addWidget(deleteButton); footerLayout->addWidget(saveButton);
    footerLayout->addWidget(editButton);
    footerLayout->addWidget(applyButton); footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(resetButton);
    connect(backButton, SIGNAL(clicked()), this, SLOT(showThumbnailPage()));
    connect(cropButton, SIGNAL(clicked()), this, SLOT(startCrop()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(deletePhoto()));
    connect(applyButton, SIGNAL(clicked()), this, SLOT(applyCrop()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelCrop()));
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetPhoto()));
    connect(saveButton, SIGNAL(clicked()), this, SLOT(savePhoto()));
    detailLayout->addWidget(footerBar);
    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setBackgroundBrush(QColor(0x0b, 0x0b, 0x0f));
    m_proxy = m_scene->addWidget(central);
    setCentralWidget(m_view);
    central->setStyleSheet(QStringLiteral(
        "QWidget{background:#0b0b0f;} QLabel{background:transparent;color:#f5f5f7;}"
        "QLabel#title{font-size:18px;font-weight:700;} QLabel#status,QLabel#videoTime{color:#a1a1aa;}"
        "QLabel#homeTime{font-size:52px;font-weight:200;color:#ffffff;margin-top:14px;}"
        "QLabel#appName{font-size:15px;font-weight:700;color:#ffffff;}"
        "QLabel#homeInfo{font-size:18px;font-weight:600;color:#ffffff;}"
        "QPushButton#appIcon{background:transparent;border:0;border-radius:20px;}"
        "QLabel#touchCount,QLabel#touchSlide{color:#30d158;font-weight:700;}"
        "QLabel#infoPanel{background:rgba(0,0,0,0.68);color:#f5f5f7;border-radius:6px;padding:6px;}"
        "QPushButton{color:#f5f5f7;background:rgba(255,255,255,0.12);"
        "border:0;border-radius:8px;padding:0 8px;}"
        "QPushButton:pressed{background:rgba(255,255,255,0.26);}"
        "QPushButton#primaryButton{background:#0a84ff;}"
        "QPushButton#dangerButton{background:#ff453a;}"
        "ThumbnailButton#thumbButton{border:0;}"
        "QScrollArea{border:0;background:transparent;}"
        "QScrollBar:vertical{background:rgba(255,255,255,0.06);width:22px;margin:0;}"
        "QScrollBar::handle:vertical{background:rgba(255,255,255,0.35);"
        "border-radius:6px;min-height:40px;}"
        "QScrollBar:horizontal{background:rgba(255,255,255,0.06);height:22px;margin:0;}"
        "QScrollBar::handle:horizontal{background:rgba(255,255,255,0.35);"
        "border-radius:6px;min-width:40px;}"
        "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
        "QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}"
        "QSlider::groove:horizontal{height:8px;background:rgba(255,255,255,0.2);"
        "border-radius:4px;}"
        "QSlider::sub-page:horizontal{background:#0a84ff;border-radius:4px;}"
        "QSlider::handle:horizontal{width:32px;height:32px;margin:-12px 0;"
        "border-radius:16px;background:#0a84ff;border:2px solid rgba(255,255,255,0.6);}"));
    loadWatcher = new QFutureWatcher<QImage>(this);
    connect(loadWatcher, SIGNAL(finished()), this, SLOT(onImageLoaded()));
    showHomePage();
}

void MainWindow::showHomePage()
{
    monitorRefreshTimer->stop();
    stopMonitorMjpeg();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(homePage);
}

void MainWindow::showMonitorPage()
{
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(monitorPage);
    startMonitorMjpeg();
    refreshMonitorSnapshots();
    monitorRefreshTimer->start();
}

QString MainWindow::gridServerBase() const
{
    const QStringList configCandidates = QStringList()
            << QCoreApplication::applicationDirPath() + QStringLiteral("/gridServer.txt")
            << QDir::currentPath() + QStringLiteral("/gridServer.txt");
    foreach (const QString &candidate, configCandidates) {
        QFile file(candidate);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString base = QString::fromUtf8(file.readAll()).trimmed();
            if (!base.isEmpty())
                return base;
        }
    }
    return QStringLiteral("http://192.168.137.1:8010");
}

void MainWindow::stopMonitorMjpeg()
{
    foreach (QNetworkReply *reply, monitorMjpegReplies) {
        reply->abort();
        reply->deleteLater();
    }
    monitorMjpegReplies.clear();
    monitorMjpegBuffers.clear();
}

void MainWindow::startMonitorMjpeg()
{
    stopMonitorMjpeg();
    const QString base = gridServerBase();
    if (base.isEmpty())
        return;
    for (int i = 0; i < monitorCells.size(); ++i) {
        const QString url = base + QStringLiteral("/cam%1").arg(i + 1);
        QNetworkReply *reply = networkManager->get(QNetworkRequest{QUrl(url)});
        monitorMjpegReplies.append(reply);
        monitorMjpegBuffers.append(QByteArray());
        const int index = i;
        connect(reply, &QNetworkReply::readyRead, this, [this, reply, index]() {
            if (index >= monitorMjpegBuffers.size())
                return;
            QByteArray &buf = monitorMjpegBuffers[index];
            buf.append(reply->readAll());
            if (buf.size() > 256 * 1024)
                buf.clear();
            const int start = buf.lastIndexOf(QByteArray("\xFF\xD8", 2));
            if (start < 0)
                return;
            const int end = buf.indexOf(QByteArray("\xFF\xD9", 2), start + 2);
            if (end < 0)
                return;
            const QByteArray jpg = buf.mid(start, end - start + 2);
            buf.clear();
            QImage image;
            if (!image.loadFromData(jpg, "JPG"))
                return;
            if (index >= monitorCells.size())
                return;
            QPushButton *cell = monitorCells[index];
            QSize target = cell->size();
            if (target.width() < 8 || target.height() < 8)
                target = QSize(158, 77);
            QImage cover = image.scaled(target, Qt::KeepAspectRatioByExpanding,
                                        Qt::FastTransformation);
            if (cover.width() > target.width() || cover.height() > target.height())
                cover = cover.copy((cover.width() - target.width()) / 2,
                                   (cover.height() - target.height()) / 2,
                                   target.width(), target.height());
            cell->setIcon(QIcon(QPixmap::fromImage(cover)));
            cell->setIconSize(target);
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, index]() {
            reply->deleteLater();
            monitorMjpegReplies.removeAll(reply);
            Q_UNUSED(index);
        });
    }
}

void MainWindow::buildMonitorPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 2, 4, 2);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("监控"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    QGridLayout *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(3);
    const int channels = 9;
    for (int i = 0; i < channels; ++i) {
        QPushButton *cell = new QPushButton(page);
        cell->setObjectName(QStringLiteral("thumbButton"));
        cell->setProperty("monitorIndex", i);
        cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(cell, &QPushButton::clicked, this, [this, i]() { openMonitorChannel(i); });
        grid->addWidget(cell, i / 3, i % 3);
        monitorCells.append(cell);
    }
    for (int r = 0; r < 3; ++r)
        grid->setRowStretch(r, 1);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);
    layout->addLayout(grid, 1);

    monitorRefreshTimer = new QTimer(this);
    monitorRefreshTimer->setInterval(4000);
    connect(monitorRefreshTimer, &QTimer::timeout,
            this, &MainWindow::refreshMonitorSnapshots);
}

QString MainWindow::monitorLiveBase() const
{
    const QStringList streams = findStreams();
    foreach (const QString &url, streams) {
        if (url.startsWith(QStringLiteral("http://"))) {
            const int slash = url.indexOf(QLatin1Char('/'), 7);
            if (slash > 0)
                return url.left(slash);
        }
    }
    return QString();
}

QString MainWindow::monitorServerBase() const
{
    return streamServerBase();
}

void MainWindow::refreshMonitorSnapshots()
{
    const QString base = monitorServerBase();
    if (base.isEmpty())
        return;
    foreach (QNetworkReply *reply, monitorReplies)
        reply->abort();
    monitorReplies.clear();
    for (int i = 0; i < monitorCells.size(); ++i) {
        const QString url = base + QStringLiteral("/snap/cam%1.jpg").arg(i + 1);
        QNetworkRequest request{QUrl(url)};
        QNetworkReply *reply = networkManager->get(request);
        monitorReplies.append(reply);
        const int index = i;
        connect(reply, &QNetworkReply::finished, this, [this, reply, index]() {
            reply->deleteLater();
            monitorReplies.removeAll(reply);
            if (reply->error() != QNetworkReply::NoError)
                return;
            QImage image;
            if (image.loadFromData(reply->readAll(), "JPG")) {
                if (index < monitorCells.size()) {
                    QPushButton *cell = monitorCells[index];
                    QSize target = cell->size();
                    if (target.width() < 8 || target.height() < 8)
                        target = QSize(158, 77);
                    QImage cover = image.scaled(target, Qt::KeepAspectRatioByExpanding,
                                                Qt::FastTransformation);
                    if (cover.width() > target.width() || cover.height() > target.height())
                        cover = cover.copy((cover.width() - target.width()) / 2,
                                           (cover.height() - target.height()) / 2,
                                           target.width(), target.height());
                    cell->setIcon(QIcon(QPixmap::fromImage(cover)));
                    cell->setIconSize(target);
                }
            }
        });
    }
}

void MainWindow::openMonitorChannel(int index)
{
    stopMonitorMjpeg();
    const QString base = monitorLiveBase();
    if (base.isEmpty())
        return;
    const QString url = base + QStringLiteral("/live/cam%1/index.m3u8").arg(index + 1);
    monitorRefreshTimer->stop();
    videoReturnPage = monitorPage;
    videoTitle->setText(tr("摄像头 %1").arg(index + 1));
    videoPlayButton->setText(tr("暂停"));
    videoSeekSlider->setVisible(false);
    videoTimeLabel->setVisible(false);
    videoDeleteButton->setVisible(false);
    videoPlayer->setRecordingStart(QDateTime());
    videoPlayer->open(url);
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(videoPage);
}

namespace {
// 有网络 SVG 的应用返回相对路径，无则返回空（走程序绘制兜底）
QString desktopSvgName(const QString &appId)
{
    static const QHash<QString, QString> table{
        {QStringLiteral("album"), QStringLiteral("icons/album.svg")},
        {QStringLiteral("monitor"), QStringLiteral("icons/monitor.svg")},
        {QStringLiteral("video"), QStringLiteral("icons/video.svg")},
        {QStringLiteral("game2048"), QStringLiteral("icons/game2048.svg")},
        {QStringLiteral("music"), QStringLiteral("icons/music.svg")},
        {QStringLiteral("calc"), QStringLiteral("icons/calc.svg")},
        {QStringLiteral("draw"), QStringLiteral("icons/draw.svg")},
        {QStringLiteral("settings"), QStringLiteral("icons/settings.svg")},
        {QStringLiteral("debug"), QStringLiteral("icons/debug.svg")},
    };
    return table.value(appId);
}

QColor desktopTint(const QString &appId)
{
    static const QHash<QString, QColor> table{
        {QStringLiteral("album"), QColor(0x0a, 0x84, 0xff)},
        {QStringLiteral("monitor"), QColor(0x34, 0xc7, 0x59)},
        {QStringLiteral("video"), QColor(0xff, 0x3b, 0x30)},
        {QStringLiteral("game"), QColor(0xff, 0x95, 0x00)},
        {QStringLiteral("snake"), QColor(0xaf, 0x52, 0xde)},
        {QStringLiteral("tetris"), QColor(0xff, 0xcc, 0x00)},
        {QStringLiteral("brick"), QColor(0xff, 0x2d, 0x55)},
        {QStringLiteral("game2048"), QColor(0x5a, 0xc8, 0xfa)},
        {QStringLiteral("music"), QColor(0xff, 0x37, 0x5f)},
        {QStringLiteral("calc"), QColor(0x8e, 0x8e, 0x93)},
        {QStringLiteral("draw"), QColor(0xff, 0x95, 0x00)},
        {QStringLiteral("settings"), QColor(0x58, 0x56, 0xd6)},
        {QStringLiteral("debug"), QColor(0x30, 0xd1, 0x58)},
    };
    return table.value(appId, QColor(0x0a, 0x84, 0xff));
}

// 定位 SVG 图标（程序目录/icons/ 或 demo_media/icons/），找不到返回空
QString locateSvg(const QString &rel)
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString &cand : {appDir.filePath(rel),
                                appDir.filePath(QStringLiteral("demo_media/") + rel)}) {
        if (QFile::exists(cand))
            return cand;
    }
    return QString();
}

// iOS 风格应用图标：彩色渐变圆角底 + 白色线条 SVG
QPixmap makeDesktopIcon(const QString &svgAbs, const QColor &tint)
{
    QPixmap pm(132, 132);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0, 0, 0, 132);
    grad.setColorAt(0, tint.lighter(165));
    grad.setColorAt(0.55, tint.lighter(120));
    grad.setColorAt(1, tint.darker(135));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(QRectF(2, 2, 128, 128), 30, 30);
    p.setBrush(QColor(255, 255, 255, 42));   // 顶缘高光
    p.drawRoundedRect(QRectF(6, 4, 120, 52), 24, 24);
    const QIcon ico(svgAbs);
    if (!ico.isNull())
        p.drawPixmap((132 - 92) / 2, (132 - 92) / 2, ico.pixmap(92, 92));
    p.end();
    return pm;
}
} // namespace

void MainWindow::buildHomePage(QWidget *page)
{
    // 背景：cover 等比放大填满 + 居中裁切，避免拉伸变形
    QLabel *bg = new QLabel(page);
    QPixmap wp(Config::kWallpaper);
    const QSize screenSize(1024, 600);
    if (!wp.isNull()) {
        const qreal scale = qMax(qreal(screenSize.width()) / wp.width(),
                                 qreal(screenSize.height()) / wp.height());
        QPixmap cover = wp.scaled(qRound(wp.width() * scale), qRound(wp.height() * scale),
                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
        cover = cover.copy((cover.width() - screenSize.width()) / 2,
                           (cover.height() - screenSize.height()) / 2,
                           screenSize.width(), screenSize.height());
        bg->setPixmap(cover);
        bg->setScaledContents(false);
    }
    bg->setGeometry(0, 0, screenSize.width(), screenSize.height());
    bg->setAttribute(Qt::WA_TransparentForMouseEvents);
    bg->lower();

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 8, 16, 16);

    QLabel *timeLabel = new QLabel(page);
    timeLabel->setObjectName(QStringLiteral("homeTime"));
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setText(QTime::currentTime().toString(QStringLiteral("HH:mm")));
    QTimer *clock = new QTimer(page);
    clock->setInterval(1000);
    connect(clock, &QTimer::timeout, timeLabel, [timeLabel]() {
        timeLabel->setText(QTime::currentTime().toString(QStringLiteral("HH:mm")));
    });
    clock->start();
    layout->addWidget(timeLabel);

    QHBoxLayout *infoRow = new QHBoxLayout();
    weatherLabel = new QLabel(tr("天气加载中..."), page);
    weatherLabel->setObjectName(QStringLiteral("homeInfo"));
    weatherLabel->setAlignment(Qt::AlignCenter);
    alsLabel = new QLabel(tr("光照 --"), page);
    alsLabel->setObjectName(QStringLiteral("homeInfo"));
    alsLabel->setAlignment(Qt::AlignCenter);
    infoRow->addStretch();
    infoRow->addWidget(weatherLabel);
    infoRow->addSpacing(40);
    infoRow->addWidget(alsLabel);
    infoRow->addStretch();
    layout->addLayout(infoRow);

    HomePageView *view = new HomePageView(page);

    // 页1：相册/监控/视频 · 坦克大战/贪吃蛇/俄罗斯方块
    QWidget *page1 = new QWidget;
    {
        QGridLayout *g = new QGridLayout(page1);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(30);
        g->setVerticalSpacing(24);
        g->addWidget(makeAppCell(tr("相册"), QStringLiteral("album"), desktopTint("album"), desktopSvgName("album"), IconFactory::album(), SLOT(showThumbnailPage())), 0, 0);
        g->addWidget(makeAppCell(tr("监控"), QStringLiteral("monitor"), desktopTint("monitor"), desktopSvgName("monitor"), IconFactory::monitor(), SLOT(showMonitorPage())), 0, 1);
        g->addWidget(makeAppCell(tr("视频"), QStringLiteral("video"), desktopTint("video"), desktopSvgName("video"), IconFactory::video(), SLOT(showVideoListPage())), 0, 2);
        g->addWidget(makeAppCell(tr("坦克大战"), QStringLiteral("game"), desktopTint("game"), desktopSvgName("game"), IconFactory::game(), SLOT(showLevelSelectPage())), 1, 0);
        g->addWidget(makeAppCell(tr("贪吃蛇"), QStringLiteral("snake"), desktopTint("snake"), desktopSvgName("snake"), IconFactory::snake(), SLOT(showSnakePage())), 1, 1);
        g->addWidget(makeAppCell(tr("俄罗斯方块"), QStringLiteral("tetris"), desktopTint("tetris"), desktopSvgName("tetris"), IconFactory::tetris(), SLOT(showTetrisPage())), 1, 2);
        view->addPage(page1);
    }

    // 页2：打砖块/2048/音乐 · 计算器/画板/设置
    QWidget *page2 = new QWidget;
    {
        QGridLayout *g = new QGridLayout(page2);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(30);
        g->setVerticalSpacing(24);
        g->addWidget(makeAppCell(tr("打砖块"), QStringLiteral("brick"), desktopTint("brick"), desktopSvgName("brick"), IconFactory::brick(), SLOT(showBrickPage())), 0, 0);
        g->addWidget(makeAppCell(tr("2048"), QStringLiteral("game2048"), desktopTint("game2048"), desktopSvgName("game2048"), IconFactory::game2048(), SLOT(showGame2048Page())), 0, 1);
        g->addWidget(makeAppCell(tr("音乐"), QStringLiteral("music"), desktopTint("music"), desktopSvgName("music"), IconFactory::music(), SLOT(showMusicPage())), 0, 2);
        g->addWidget(makeAppCell(tr("计算器"), QStringLiteral("calc"), desktopTint("calc"), desktopSvgName("calc"), IconFactory::calculator(), SLOT(showCalculatorPage())), 1, 0);
        g->addWidget(makeAppCell(tr("画板"), QStringLiteral("draw"), desktopTint("draw"), desktopSvgName("draw"), IconFactory::draw(), SLOT(showDrawPage())), 1, 1);
        g->addWidget(makeAppCell(tr("设置"), QStringLiteral("settings"), desktopTint("settings"), desktopSvgName("settings"), IconFactory::settings(), SLOT(showSettingsPage())), 1, 2);
        view->addPage(page2);
    }

    // 页3：调试（单格居中）
    QWidget *page3 = new QWidget;
    {
        QGridLayout *g = new QGridLayout(page3);
        g->setContentsMargins(0, 0, 0, 0);
        g->addWidget(makeAppCell(tr("调试"), QStringLiteral("debug"), desktopTint("debug"), desktopSvgName("debug"), IconFactory::debug(), SLOT(showDebugPage())), 0, 1);
        view->addPage(page3);
    }

    layout->addStretch(1);
    layout->addWidget(view, 1);
    layout->addStretch(1);
}

QWidget *MainWindow::makeAppCell(const QString &name, const QString &appId,
                                 const QColor &tint, const QString &svgRel,
                                 const QPixmap &fallbackIcon, const char *slot)
{
    QWidget *cell = new QWidget;
    cell->setStyleSheet(QStringLiteral("background:transparent;"));
    QVBoxLayout *v = new QVBoxLayout(cell);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    QPushButton *button = new QPushButton(cell);
    button->setProperty("app", appId);
    const QSize iconSize(110, 110);
    QPixmap icon;
    const QString svgAbs = locateSvg(svgRel);
    if (!svgAbs.isEmpty()) {
        icon = makeDesktopIcon(svgAbs, tint);   // iOS 彩色底 + 白色 SVG
    } else {
        icon = fallbackIcon.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    button->setIcon(QIcon(icon));
    button->setIconSize(iconSize);
    button->setFixedSize(iconSize);
    button->setFlat(true);
    button->setStyleSheet(QStringLiteral("background:transparent;border:0;padding:0;"));
    connect(button, SIGNAL(clicked()), this, slot);
    QLabel *label = new QLabel(name, cell);
    label->setObjectName(QStringLiteral("appName"));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("background:transparent;"));
    v->addWidget(button, 0, Qt::AlignHCenter);
    v->addWidget(label, 0, Qt::AlignHCenter);
    return cell;
}

void MainWindow::buildThumbnailPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *titleRow = new QHBoxLayout();
    QLabel *label = new QLabel(tr("照片"), page);
    label->setObjectName(QStringLiteral("title"));
    titleRow->addWidget(label);
    titleRow->addStretch();
    QPushButton *homeButton = new QPushButton(tr("主页"), page);
    homeButton->setMinimumSize(48, 32);
    connect(homeButton, SIGNAL(clicked()), this, SLOT(showHomePage()));
    titleRow->addWidget(homeButton);
    layout->addLayout(titleRow);
    QScrollArea *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    QWidget *content = new QWidget(scroll);
    thumbnailGrid = new QGridLayout(content);
    thumbnailGrid->setContentsMargins(0, 0, 0, 0);
    thumbnailGrid->setSpacing(2);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
    gridDragScroll = new ThumbDragScroll(scroll, this);
    gridDragScroll->attach(content);
}

void MainWindow::onVerticalSwipe(bool upward)
{
    if (photoView->viewMode() == PhotoView::CropMode)
        return;
    if (upward) {
        if (infoPanel->isVisible()) {
            infoPanel->setVisible(false);
            return;
        }
        const QString path = photoPaths.value(currentIndex);
        if (path.startsWith(QStringLiteral("demo://"))) {
            infoPanel->setText(tr("演示图 %1").arg(currentIndex + 1));
        } else {
            const QFileInfo info(path);
            const QSize size = photoView->imageSize();
            infoPanel->setText(tr("文件：%1\n尺寸：%2×%3\n修改时间：%4")
                .arg(info.fileName())
                .arg(size.width()).arg(size.height())
                .arg(info.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
        }
        infoPanel->setVisible(true);
        footerBar->setVisible(false);
    } else {
        infoPanel->setVisible(false);
        footerBar->setVisible(!footerBar->isVisible());
    }
}

void MainWindow::rebuildThumbnailGrid()
{
    if (!thumbnailGrid)
        return;
    while (QLayoutItem *item = thumbnailGrid->takeAt(0)) {
        delete item->widget(); delete item;
    }
    QWidget *content = thumbnailGrid->parentWidget();
    int baseW = (m_proxy && m_proxy->widget()) ? m_proxy->widget()->width() : width();
    if (QScrollArea *scroll = qobject_cast<QScrollArea *>(content->parentWidget())) {
        const int vw = scroll->viewport()->width();
        if (vw >= 100)
            baseW = vw;
    }
    const int cols = (baseW >= 900) ? 5 : (baseW >= 650) ? 4 : (baseW >= 450) ? 3 : 2;
    const int cellW = (baseW - (cols - 1) * 2) / cols;
    const QSize cell(cellW, cellW * 3 / 4);
    for (int index = 0; index < photoPaths.size(); ++index) {
        ThumbnailButton *button = new ThumbnailButton(content);
        button->setObjectName(QStringLiteral("thumbButton"));
        button->setFixedSize(cell);
        button->setThumbnail(thumbnailForPath(photoPaths.at(index), cell));
        button->setProperty("photoIndex", QVariant(index));
        connect(button, &ThumbnailButton::clicked, this, [this, index]() { showDetailPage(index); });
        if (gridDragScroll)
            gridDragScroll->attach(button);
        thumbnailGrid->addWidget(button, index / cols, index % cols);
    }
    const int rows = (photoPaths.size() + cols - 1) / cols;
    for (int r = 0; r <= 30; ++r)
        thumbnailGrid->setRowStretch(r, 0);
    thumbnailGrid->setRowStretch(rows, 1);
}

void MainWindow::showThumbnailPage()
{
    monitorRefreshTimer->stop();
    rebuildThumbnailGrid();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(thumbnailPage);
}

void MainWindow::showDetailPage(int index)
{
    currentIndex = index;
    showPhoto();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(detailPage);
}

void MainWindow::buildVideoListPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("视频"), page);
    title->setObjectName(QStringLiteral("title"));
    headerRow->addWidget(back);
    headerRow->addSpacing(8);
    headerRow->addWidget(title);
    headerRow->addStretch();
    layout->addLayout(headerRow);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    videoList = new QListWidget(page);
    videoList->setObjectName(QStringLiteral("videoList"));
    videoList->setStyleSheet(QStringLiteral(
        "QListWidget{background:#14141a;border:0;border-radius:8px;color:#f5f5f7;}"
        "QListWidget::item{padding:10px 8px;border-bottom:1px solid rgba(255,255,255,0.08);}"
        "QListWidget::item:selected{background:#0a84ff;}"));
    connect(videoList, SIGNAL(currentRowChanged(int)),
            this, SLOT(openVideo(int)));
    layout->addWidget(videoList, 1);
}

void MainWindow::buildVideoPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    videoTitle = new QLabel(tr("视频"), page);
    videoTitle->setObjectName(QStringLiteral("title"));
    videoPlayButton = new QPushButton(tr("暂停"), page);
    videoPlayButton->setMinimumSize(48, 32);
    videoMuteButton = new QPushButton(tr("静音"), page);
    videoMuteButton->setMinimumSize(48, 32);
    headerRow->addWidget(back);
    headerRow->addSpacing(8);
    headerRow->addWidget(videoTitle, 1);
    headerRow->addWidget(videoPlayButton);
    headerRow->addWidget(videoMuteButton);
    layout->addLayout(headerRow);

    videoPlayer = new VideoPlayerWidget(page);
    connect(videoPlayer, SIGNAL(backRequested()), this, SLOT(videoBack()));
    layout->addWidget(videoPlayer, 1);
    connect(back, SIGNAL(clicked()), this, SLOT(videoBack()));
    connect(videoPlayButton, SIGNAL(clicked()), this, SLOT(toggleVideoPlay()));
    connect(videoMuteButton, SIGNAL(clicked()), this, SLOT(toggleVideoMute()));

    QHBoxLayout *seekRow = new QHBoxLayout();
    videoSeekSlider = new QSlider(Qt::Horizontal, page);
    videoSeekSlider->setObjectName(QStringLiteral("videoSeekSlider"));
    videoTimeLabel = new QLabel(tr("00:00/00:00"), page);
    videoTimeLabel->setObjectName(QStringLiteral("videoTime"));
    videoDeleteButton = new QPushButton(tr("删除"), page);
    videoDeleteButton->setMinimumSize(48, 32);
    videoDeleteButton->setObjectName(QStringLiteral("dangerButton"));
    seekRow->addWidget(videoSeekSlider, 1);
    seekRow->addWidget(videoTimeLabel);
    seekRow->addWidget(videoDeleteButton);
    layout->addLayout(seekRow);
    videoSeekSlider->setVisible(false);
    videoTimeLabel->setVisible(false);
    videoDeleteButton->setVisible(false);
    connect(videoSeekSlider, &QSlider::sliderPressed, this, [this]() { sliderDragging = true; });
    connect(videoSeekSlider, &QSlider::sliderMoved, this, &MainWindow::onVideoSeek);
    connect(videoSeekSlider, &QSlider::sliderReleased, this, [this]() {
        sliderDragging = false;
        onVideoSeek(videoSeekSlider->value());
    });
    connect(videoPlayer, SIGNAL(positionChanged(qint64)),
            this, SLOT(onVideoPosition(qint64)));
    connect(videoPlayer, SIGNAL(durationChanged(qint64)),
            this, SLOT(onVideoDuration(qint64)));
    connect(videoDeleteButton, SIGNAL(clicked()), this, SLOT(deleteRecording()));

    networkManager = new QNetworkAccessManager(this);
}

void MainWindow::buildGamePage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("坦克大战"), page);
    title->setObjectName(QStringLiteral("title"));
    headerRow->addWidget(back);
    headerRow->addSpacing(8);
    headerRow->addWidget(title);
    headerRow->addStretch();
    layout->addLayout(headerRow);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    tankGame = new TankGame(page);

    QGridLayout *mainArea = new QGridLayout();
    mainArea->setContentsMargins(8, 0, 8, 4);
    mainArea->setVerticalSpacing(0);
    mainArea->setHorizontalSpacing(8);

    QProgressBar *healthBar = new QProgressBar(page);
    healthBar->setRange(0, 100);
    healthBar->setValue(100);
    healthBar->setFixedSize(130, 16);
    healthBar->setTextVisible(false);
    QProgressBar *manaBar = new QProgressBar(page);
    manaBar->setRange(0, 100);
    manaBar->setValue(100);
    manaBar->setFixedSize(130, 16);
    manaBar->setTextVisible(false);
    QVBoxLayout *topLeft = new QVBoxLayout();
    topLeft->setSpacing(3);
    topLeft->addWidget(healthBar);
    topLeft->addWidget(manaBar);
    topLeft->addStretch();
    mainArea->addLayout(topLeft, 0, 0, Qt::AlignTop);

    QGridLayout *dpad = new QGridLayout();
    dpad->setSpacing(4);
    QPushButton *up = new QPushButton(tr("上"), page);
    QPushButton *down = new QPushButton(tr("下"), page);
    QPushButton *left = new QPushButton(tr("左"), page);
    QPushButton *right = new QPushButton(tr("右"), page);
    QList<QPushButton *> dpadBtns;
    dpadBtns << up << down << left << right;
    foreach (QPushButton *b, dpadBtns)
        b->setFixedSize(64, 48);
    dpad->addWidget(up, 0, 1);
    dpad->addWidget(left, 1, 0);
    dpad->addWidget(right, 1, 2);
    dpad->addWidget(down, 2, 1);
    mainArea->addLayout(dpad, 1, 0, Qt::AlignLeft | Qt::AlignBottom);

    mainArea->addWidget(tankGame, 0, 1, 2, 1);

    QPushButton *fire = new QPushButton(tr("攻击"), page);
    QPushButton *skill1 = new QPushButton(tr("激光"), page);
    QPushButton *skill2 = new QPushButton(tr("炸弹"), page);
    fire->setObjectName(QStringLiteral("dangerButton"));
    skill1->setObjectName(QStringLiteral("primaryButton"));
    skill2->setObjectName(QStringLiteral("primaryButton"));
    QList<QPushButton *> actBtns;
    actBtns << fire << skill1 << skill2;
    foreach (QPushButton *b, actBtns)
        b->setFixedSize(64, 48);
    QVBoxLayout *bottomRight = new QVBoxLayout();
    bottomRight->setSpacing(6);
    bottomRight->addStretch();
    bottomRight->addWidget(fire);
    bottomRight->addWidget(skill1);
    bottomRight->addWidget(skill2);
    mainArea->addLayout(bottomRight, 1, 2, Qt::AlignRight | Qt::AlignBottom);

    mainArea->setColumnStretch(0, 0);
    mainArea->setColumnStretch(1, 1);
    mainArea->setColumnStretch(2, 0);

    layout->addLayout(mainArea, 1);

    connect(up, &QPushButton::pressed, this, [this]() { tankGame->setMoveDir(TankGame::DirUp); });
    connect(down, &QPushButton::pressed, this, [this]() { tankGame->setMoveDir(TankGame::DirDown); });
    connect(left, &QPushButton::pressed, this, [this]() { tankGame->setMoveDir(TankGame::DirLeft); });
    connect(right, &QPushButton::pressed, this, [this]() { tankGame->setMoveDir(TankGame::DirRight); });
    connect(up, &QPushButton::released, this, [this]() { tankGame->setMoveDir(TankGame::DirNone); });
    connect(down, &QPushButton::released, this, [this]() { tankGame->setMoveDir(TankGame::DirNone); });
    connect(left, &QPushButton::released, this, [this]() { tankGame->setMoveDir(TankGame::DirNone); });
    connect(right, &QPushButton::released, this, [this]() { tankGame->setMoveDir(TankGame::DirNone); });
    connect(fire, &QPushButton::clicked, this, [this]() { tankGame->fire(); });
    connect(skill1, &QPushButton::clicked, this, [this]() { tankGame->skillLaser(); });
    connect(skill2, &QPushButton::clicked, this, [this]() { tankGame->skillBomb(); });
    connect(tankGame, &TankGame::healthChanged, healthBar, &QProgressBar::setValue);
    connect(tankGame, &TankGame::manaChanged, manaBar, &QProgressBar::setValue);
    connect(tankGame, &TankGame::gameOver, this, [title](bool win) {
        title->setText(win ? tr("坦克大战 - 胜利") : tr("坦克大战 - 失败"));
    });
}

void MainWindow::buildLevelSelectPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("选择关卡"), page);
    title->setObjectName(QStringLiteral("title"));
    headerRow->addWidget(back);
    headerRow->addSpacing(8);
    headerRow->addWidget(title);
    headerRow->addStretch();
    layout->addLayout(headerRow);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    for (int i = 0; i < 10; ++i) {
        QPushButton *btn = new QPushButton(tr("第 %1 关").arg(i + 1), page);
        btn->setMinimumSize(140, 60);
        const int level = i + 1;
        connect(btn, &QPushButton::clicked, this, [this, level]() { showGamePage(level); });
        grid->addWidget(btn, i / 5, i % 5);
    }
    layout->addLayout(grid);
    layout->addStretch();
}

void MainWindow::showLevelSelectPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(levelSelectPage);
}

void MainWindow::showGamePage(int level)
{
    stopVideo();
    monitorRefreshTimer->stop();
    if (tankGame) {
        tankGame->setLevel(level);
        tankGame->startGame();
    }
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(gamePage);
}

void MainWindow::buildSnakePage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("贪吃蛇"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    snakeGame = new SnakeGame(page);
    layout->addWidget(snakeGame, 1);

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addStretch();
    QPushButton *up = new QPushButton(tr("上"), page);
    QPushButton *down = new QPushButton(tr("下"), page);
    QPushButton *left = new QPushButton(tr("左"), page);
    QPushButton *right = new QPushButton(tr("右"), page);
    QList<QPushButton *> btns;
    btns << up << down << left << right;
    foreach (QPushButton *b, btns)
        b->setMinimumSize(64, 48);
    controls->addWidget(up);
    controls->addWidget(left);
    controls->addWidget(down);
    controls->addWidget(right);
    controls->addStretch();
    layout->addLayout(controls);

    connect(up, &QPushButton::clicked, this, [this]() { snakeGame->setDirection(SnakeGame::DirUp); });
    connect(down, &QPushButton::clicked, this, [this]() { snakeGame->setDirection(SnakeGame::DirDown); });
    connect(left, &QPushButton::clicked, this, [this]() { snakeGame->setDirection(SnakeGame::DirLeft); });
    connect(right, &QPushButton::clicked, this, [this]() { snakeGame->setDirection(SnakeGame::DirRight); });
}

void MainWindow::showSnakePage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    if (snakeGame)
        snakeGame->startGame();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(snakePage);
}

void MainWindow::buildTetrisPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("俄罗斯方块"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    tetrisGame = new TetrisGame(page);
    layout->addWidget(tetrisGame, 1);

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addStretch();
    QPushButton *left = new QPushButton(tr("左"), page);
    QPushButton *right = new QPushButton(tr("右"), page);
    QPushButton *rotate = new QPushButton(tr("旋转"), page);
    QPushButton *drop = new QPushButton(tr("下"), page);
    QList<QPushButton *> btns;
    btns << left << right << rotate << drop;
    foreach (QPushButton *b, btns)
        b->setMinimumSize(64, 48);
    controls->addWidget(left);
    controls->addWidget(rotate);
    controls->addWidget(drop);
    controls->addWidget(right);
    controls->addStretch();
    layout->addLayout(controls);

    connect(left, &QPushButton::clicked, this, [this]() { tetrisGame->moveLeft(); });
    connect(right, &QPushButton::clicked, this, [this]() { tetrisGame->moveRight(); });
    connect(rotate, &QPushButton::clicked, this, [this]() { tetrisGame->rotate(); });
    connect(drop, &QPushButton::clicked, this, [this]() { tetrisGame->softDrop(); });
}

void MainWindow::showTetrisPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    if (tetrisGame)
        tetrisGame->startGame();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(tetrisPage);
}

void MainWindow::buildBrickPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("打砖块"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    brickGame = new BrickGame(page);
    layout->addWidget(brickGame, 1);

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addStretch();
    QPushButton *left = new QPushButton(tr("左"), page);
    QPushButton *right = new QPushButton(tr("右"), page);
    left->setMinimumSize(64, 48);
    right->setMinimumSize(64, 48);
    controls->addWidget(left);
    controls->addSpacing(20);
    controls->addWidget(right);
    controls->addStretch();
    layout->addLayout(controls);

    connect(left, &QPushButton::pressed, this, [this]() { brickGame->setPaddleDir(-1); });
    connect(left, &QPushButton::released, this, [this]() { brickGame->setPaddleDir(0); });
    connect(right, &QPushButton::pressed, this, [this]() { brickGame->setPaddleDir(1); });
    connect(right, &QPushButton::released, this, [this]() { brickGame->setPaddleDir(0); });
}

void MainWindow::showBrickPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    if (brickGame)
        brickGame->startGame();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(brickPage);
}

void MainWindow::buildGame2048Page(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("2048"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    game2048 = new Game2048(page);
    layout->addWidget(game2048, 1);

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addStretch();
    QPushButton *up = new QPushButton(tr("上"), page);
    QPushButton *down = new QPushButton(tr("下"), page);
    QPushButton *left = new QPushButton(tr("左"), page);
    QPushButton *right = new QPushButton(tr("右"), page);
    QList<QPushButton *> btns;
    btns << up << down << left << right;
    foreach (QPushButton *b, btns)
        b->setMinimumSize(64, 48);
    controls->addWidget(up);
    controls->addWidget(left);
    controls->addWidget(down);
    controls->addWidget(right);
    controls->addStretch();
    layout->addLayout(controls);

    connect(up, &QPushButton::clicked, this, [this]() { game2048->move(Game2048::DirUp); });
    connect(down, &QPushButton::clicked, this, [this]() { game2048->move(Game2048::DirDown); });
    connect(left, &QPushButton::clicked, this, [this]() { game2048->move(Game2048::DirLeft); });
    connect(right, &QPushButton::clicked, this, [this]() { game2048->move(Game2048::DirRight); });
}

void MainWindow::showGame2048Page()
{
    stopVideo();
    monitorRefreshTimer->stop();
    if (game2048)
        game2048->startGame();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(game2048Page);
}

void MainWindow::buildMusicPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("音乐"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    musicPlayer = new MusicPlayer(page);
    layout->addWidget(musicPlayer, 1);

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addStretch();
    QPushButton *prev = new QPushButton(tr("上一首"), page);
    QPushButton *play = new QPushButton(tr("播放/暂停"), page);
    QPushButton *next = new QPushButton(tr("下一首"), page);
    prev->setMinimumSize(64, 48);
    play->setMinimumSize(80, 48);
    next->setMinimumSize(64, 48);
    controls->addWidget(prev);
    controls->addSpacing(16);
    controls->addWidget(play);
    controls->addSpacing(16);
    controls->addWidget(next);
    controls->addStretch();
    layout->addLayout(controls);

    connect(prev, &QPushButton::clicked, this, [this]() { musicPlayer->prev(); });
    connect(play, &QPushButton::clicked, this, [this]() { musicPlayer->togglePlay(); });
    connect(next, &QPushButton::clicked, this, [this]() { musicPlayer->next(); });
}

void MainWindow::showMusicPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    if (musicPlayer)
        musicPlayer->start();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(musicPage);
}

void MainWindow::buildCalculatorPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("计算器"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    calculator = new Calculator(page);
    layout->addWidget(calculator, 1);
}

void MainWindow::showCalculatorPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(calculatorPage);
}

void MainWindow::buildDrawPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("画板"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    QPushButton *clearBtn = new QPushButton(tr("清空"), page);
    clearBtn->setMinimumSize(48, 32);
    QPushButton *saveBtn = new QPushButton(tr("保存"), page);
    saveBtn->setMinimumSize(48, 32);
    header->addWidget(clearBtn);
    header->addWidget(saveBtn);
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    drawBoard = new DrawBoard(page);
    layout->addWidget(drawBoard, 1);

    connect(clearBtn, &QPushButton::clicked, this, [this]() { drawBoard->clear(); });
    connect(saveBtn, &QPushButton::clicked, this, [this]() { drawBoard->save(); });
}

void MainWindow::showDrawPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(drawPage);
}

void MainWindow::buildSettingsPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("设置"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    QHBoxLayout *row = new QHBoxLayout();
    QLabel *label = new QLabel(tr("屏幕方向锁定"), page);
    label->setObjectName(QStringLiteral("appName"));
    QCheckBox *lockCheck = new QCheckBox(page);
    lockCheck->setChecked(m_orientationLocked);
    connect(lockCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_orientationLocked = checked;
    });
    row->addWidget(label);
    row->addStretch();
    row->addWidget(lockCheck);
    layout->addLayout(row);

    layout->addStretch();
}

void MainWindow::showSettingsPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(settingsPage);
}

void MainWindow::buildDebugPage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QHBoxLayout *header = new QHBoxLayout();
    QPushButton *back = new QPushButton(tr("返回"), page);
    back->setMinimumSize(48, 32);
    QLabel *title = new QLabel(tr("调试"), page);
    title->setObjectName(QStringLiteral("title"));
    header->addWidget(back);
    header->addSpacing(8);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);
    connect(back, SIGNAL(clicked()), this, SLOT(showHomePage()));

    debugWidget = new DebugPage(page);
    layout->addWidget(debugWidget, 1);
}

void MainWindow::showDebugPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(debugPage);
}

void MainWindow::showVideoListPage()
{
    stopVideo();
    monitorRefreshTimer->stop();
    videoList->clear();
    const QStringList videos = findVideos();
    if (videos.isEmpty())
        videoList->addItem(tr("未找到视频文件"));
    foreach (const QString &video, videos) {
        QListWidgetItem *item = new QListWidgetItem(
                QFileInfo(video).fileName(), videoList);
        item->setData(Qt::UserRole, video);
    }
    const QStringList streams = findStreams();
    foreach (const QString &url, streams) {
        QListWidgetItem *item = new QListWidgetItem(
                tr("网络流: %1").arg(url), videoList);
        item->setData(Qt::UserRole, url);
        item->setData(Qt::UserRole + 1, QStringLiteral("live"));
    }
    loadRecordings();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(videoListPage);
}

void MainWindow::openVideo(int row)
{
    if (!videoList->item(row))
        return;
    const QString path = videoList->item(row)->data(Qt::UserRole).toString();
    if (path.isEmpty())
        return;
    const QString type = videoList->item(row)->data(Qt::UserRole + 1).toString();
    const bool isRecording = (type == QStringLiteral("recording"));
    videoReturnPage = videoListPage;
    videoTitle->setText(QFileInfo(path).fileName());
    videoPlayButton->setText(tr("暂停"));

    recordingStartText.clear();
    if (isRecording) {
        const QString name = QFileInfo(path).fileName();
        const int pos = name.indexOf(QStringLiteral("record_"));
        if (pos >= 0) {
            const QDateTime start = QDateTime::fromString(
                    name.mid(pos + 7, 15), QStringLiteral("yyyyMMdd_HHmmss"));
            if (start.isValid())
                recordingStartText = start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }
    }
    videoSeekSlider->setVisible(isRecording);
    videoTimeLabel->setVisible(isRecording);
    videoDeleteButton->setVisible(isRecording);
    if (isRecording) {
        videoSeekSlider->setRange(0, 0);
        videoSeekSlider->setValue(0);
        videoTimeLabel->setText(tr("00:00/00:00"));
    }

    videoPlayer->setRecordingStart(
            recordingStartText.isEmpty() ? QDateTime()
                                          : QDateTime::fromString(recordingStartText,
                                                                  QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    videoPlayer->open(path);
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(videoPage);
}

void MainWindow::onVideoSeek(int ms)
{
    if (videoPlayer)
        videoPlayer->seek(ms);
}

void MainWindow::onVideoPosition(qint64 ms)
{
    if (!sliderDragging && videoSeekSlider) {
        videoSeekSlider->setValue(static_cast<int>(ms));
        const qint64 seconds = ms / 1000;
        const qint64 total = videoSeekSlider->maximum() / 1000;
        videoTimeLabel->setText(QStringLiteral("%1/%2")
                .arg(QTime(0, 0).addSecs(static_cast<int>(seconds)).toString(
                         QStringLiteral("mm:ss")))
                .arg(QTime(0, 0).addSecs(static_cast<int>(total)).toString(
                         QStringLiteral("mm:ss"))));
    }
}

void MainWindow::onVideoDuration(qint64 ms)
{
    if (videoSeekSlider) {
        videoSeekSlider->setRange(0, static_cast<int>(ms));
        videoTimeLabel->setText(QStringLiteral("00:00/%1")
                .arg(QTime(0, 0).addMSecs(static_cast<int>(ms)).toString(
                         QStringLiteral("mm:ss"))));
    }
}

void MainWindow::deleteRecording()
{
    const int row = videoList->currentRow();
    if (row < 0)
        return;
    const QString path = videoList->item(row)->data(Qt::UserRole).toString();
    const QString type = videoList->item(row)->data(Qt::UserRole + 1).toString();
    if (type != QStringLiteral("recording") || path.isEmpty())
        return;

    stopVideo();
    QNetworkRequest request{QUrl(path)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("photo-album"));
    QNetworkReply *reply = networkManager->deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        showVideoListPage();
    });
}

QString MainWindow::streamServerBase() const
{
    const QStringList configCandidates = QStringList()
            << QCoreApplication::applicationDirPath() + QStringLiteral("/recordServer.txt")
            << QDir::currentPath() + QStringLiteral("/recordServer.txt");
    foreach (const QString &candidate, configCandidates) {
        QFile file(candidate);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString base = QString::fromUtf8(file.readAll()).trimmed();
            if (!base.isEmpty())
                return base;
        }
    }
    const QStringList streams = findStreams();
    foreach (const QString &url, streams) {
        if (url.startsWith(QStringLiteral("http://"))) {
            const int slash = url.indexOf(QLatin1Char('/'), 7);
            if (slash > 0)
                return url.left(slash);
        }
    }
    return QString();
}

void MainWindow::loadRecordings()
{
    const QString base = streamServerBase();
    if (base.isEmpty())
        return;
    QNetworkRequest request{QUrl(base + QStringLiteral("/list"))};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("photo-album"));
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, base]() {
        Q_UNUSED(base)
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray files = doc.object().value(QStringLiteral("files")).toArray();
        foreach (const QJsonValue &value, files) {
            const QJsonObject obj = value.toObject();
            const QString name = obj.value(QStringLiteral("name")).toString();
            const QString url = obj.value(QStringLiteral("url")).toString();
            if (name.isEmpty() || url.isEmpty())
                continue;
            QListWidgetItem *item = new QListWidgetItem(tr("录像: %1").arg(name), videoList);
            item->setData(Qt::UserRole, url);
            item->setData(Qt::UserRole + 1, QStringLiteral("recording"));
        }
    });
}

void MainWindow::videoBack()
{
    stopVideo();
    monitorRefreshTimer->stop();
    static_cast<QStackedWidget *>(stackedWidget)->setCurrentWidget(
            videoReturnPage ? videoReturnPage : videoListPage);
}

void MainWindow::toggleVideoPlay()
{
    if (!videoPlayer)
        return;
    videoPlayer->togglePause();
    videoPlayButton->setText(videoPlayer->isPaused() ? tr("播放") : tr("暂停"));
}

void MainWindow::toggleVideoMute()
{
    if (!videoPlayer)
        return;
    const bool muted = videoPlayer->audioEnabled();
    videoPlayer->setAudioEnabled(!muted);
    videoMuteButton->setText(muted ? tr("静音") : tr("出声"));
}

void MainWindow::stopVideo()
{
    if (videoPlayer) {
        videoPlayer->stop();
        videoPlayButton->setText(tr("暂停"));
    }
}

QStringList MainWindow::findVideos() const
{
    QStringList videos;
    const QStringList extensions = QStringList()
            << QStringLiteral("mp4") << QStringLiteral("mkv") << QStringLiteral("avi")
            << QStringLiteral("mov") << QStringLiteral("flv") << QStringLiteral("ts")
            << QStringLiteral("m4v") << QStringLiteral("webm");
    if (!photoDirectory.isEmpty()) {
        QDir dir(photoDirectory);
        const QFileInfoList entries = dir.entryInfoList(QDir::Files);
        foreach (const QFileInfo &info, entries) {
            if (extensions.contains(info.suffix().toLower()))
                videos.append(info.absoluteFilePath());
        }
    }
    const QStringList demoCandidates = QStringList()
            << QStringLiteral("/demo_media/demo.mp4")
            << QStringLiteral("/media/demo.mp4");
    foreach (const QString &suffix, demoCandidates) {
        QString demo = QCoreApplication::applicationDirPath() + suffix;
        if (!QFile::exists(demo))
            demo = QDir::currentPath() + suffix;
        if (QFile::exists(demo)) {
            videos.append(demo);
            break;
        }
    }
    return videos;
}

QStringList MainWindow::findStreams() const
{
    QStringList streams;
    QString file;
    const QStringList candidates = QStringList()
            << QCoreApplication::applicationDirPath() + QStringLiteral("/streams.txt")
            << QDir::currentPath() + QStringLiteral("/streams.txt");
    foreach (const QString &candidate, candidates) {
        if (QFile::exists(candidate)) {
            file = candidate;
            break;
        }
    }
    if (file.isEmpty())
        return streams;

    QFile in(file);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        return streams;
    QTextStream stream(&in);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        streams.append(line);
    }
    return streams;
}

/**
 * @brief 更新接触点数量和滑动状态显示。
 * @param contactCount 当前活动接触点数量。
 * @param sliding 是否检测到滑动。
 * @param movement 最大移动距离，单位为像素。
 */
void MainWindow::updateTouchDebug(int contactCount, bool sliding, qreal movement)
{
    touchCountLabel->setText(tr("接触点:%1").arg(contactCount));
    touchSlideLabel->setText(tr("滑动:%1(%2px)")
                             .arg(sliding ? tr("是") : tr("否"))
                             .arg(qRound(movement)));
}

/**
 * @brief 扫描图片目录并加载图片路径。
 * @param directory 图片目录路径。
 */
void MainWindow::loadPhotos(const QString &directory)
{
    photoDirectory = directory;
    photoPaths.clear();
    editedImages.clear();
    thumbnailCache.clear();
    imageCache.clear();
    imageCacheOrder.clear();
    ++loadGeneration;
    currentIndex = 0;

    if (!directory.isEmpty()) {
        QDir photoDir(directory);
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        QStringList filters;
        foreach (const QByteArray &format, formats)
            filters << QStringLiteral("*.") + QString::fromLatin1(format);
        QFileInfoList entries = photoDir.entryInfoList(
                    filters, QDir::Files, QDir::Name);
        foreach (const QFileInfo &entry, entries)
            photoPaths.append(entry.absoluteFilePath());
    }

    if (photoPaths.isEmpty()) {
        photoPaths.clear();
        for (int index = 1; index <= 15; ++index)
            photoPaths.append(QStringLiteral("demo://%1").arg(index));
    }

    rebuildThumbnailGrid();
    showPhoto();
}

/**
 * @brief 显示当前索引对应的图片。大图优先取缓存，未缓存则异步解码避免阻塞界面。
 */
void MainWindow::showPhoto()
{
    infoPanel->setVisible(false);
    if (photoPaths.isEmpty()) {
        photoView->setImage(QImage());
        updateStatus();
        updateActions();
        return;
    }

    currentIndex = (currentIndex + photoPaths.size()) % photoPaths.size();
    const QString path = photoPaths.at(currentIndex);
    preloadAdjacent();

    const QImage ready = imageForPath(path);
    if (!ready.isNull()) {
        photoView->setImage(ready);
        photoView->setViewMode(PhotoView::BrowseMode);
        updateStatus();
        updateActions();
        return;
    }

    ++loadGeneration;
    loadingPath = path;
    loadWatcher->setFuture(QtConcurrent::run(this, &MainWindow::decodeImage, path));
}

/**
 * @brief 大图异步解码完成回调。仅在解码结果仍对应当前图片时应用，并写入缓存。
 */
void MainWindow::onImageLoaded()
{
    const QImage image = loadWatcher->result();
    if (image.isNull())
        return;
    if (!loadingPath.startsWith(QStringLiteral("demo://")))
        cacheImage(loadingPath, image);
    if (currentIndex >= 0 && currentIndex < photoPaths.size() &&
            photoPaths.at(currentIndex) == loadingPath) {
        photoView->setImage(image);
        photoView->setViewMode(PhotoView::BrowseMode);
        updateStatus();
        updateActions();
    }
}

void MainWindow::cacheImage(const QString &path, const QImage &image)
{
    if (imageCache.contains(path))
        return;
    imageCache.insert(path, image);
    imageCacheOrder.append(path);
    while (imageCacheOrder.size() > 6) {
        const QString oldest = imageCacheOrder.takeFirst();
        imageCache.remove(oldest);
    }
}

void MainWindow::preloadAdjacent()
{
    const int offsets[2] = { 1, -1 };
    for (int off : offsets) {
        const int idx = (currentIndex + off + photoPaths.size()) % photoPaths.size();
        const QString path = photoPaths.value(idx);
        if (path.isEmpty() || path.startsWith(QStringLiteral("demo://")))
            continue;
        if (imageCache.contains(path))
            continue;
        QtConcurrent::run([this, path]() {
            const QImage img = decodeImage(path);
            if (!img.isNull()) {
                QMetaObject::invokeMethod(this, [this, path, img]() {
                    cacheImage(path, img);
                }, Qt::QueuedConnection);
            }
        });
    }
}

void MainWindow::updateActions()
{
    const bool cropMode = photoView->viewMode() == PhotoView::CropMode;
    cropButton->setVisible(!cropMode);
    deleteButton->setVisible(!cropMode);
    saveButton->setVisible(!cropMode);
    editButton->setVisible(!cropMode);
    applyButton->setVisible(cropMode);
    cancelButton->setVisible(cropMode);
    resetButton->setVisible(cropMode);
    applyButton->setEnabled(photoView->hasCropSelection());
}

void MainWindow::updateStatus()
{
    if (photoPaths.isEmpty()) {
        titleLabel->setText(QStringLiteral("--x--"));
        statusLabel->setText(tr("无图片"));
        return;
    }

    const QString path = photoPaths.at(currentIndex);
    const QString name = path.startsWith(QStringLiteral("demo://"))
            ? tr("演示图 %1").arg(currentIndex + 1)
            : QFileInfo(path).fileName();
    QString timeText = QStringLiteral("--");
    if (!path.startsWith(QStringLiteral("demo://"))) {
        const QDateTime modified = QFileInfo(path).lastModified();
        if (modified.isValid())
            timeText = modified.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    }
    titleLabel->setText(QStringLiteral("%1x%2 · %3")
                        .arg(photoView->imageSize().width())
                        .arg(photoView->imageSize().height())
                        .arg(timeText));
    statusLabel->setText(QStringLiteral("%1/%2  %3")
                         .arg(currentIndex + 1)
                         .arg(photoPaths.size())
                         .arg(name));
}

QImage MainWindow::imageForPath(const QString &path) const
{
    if (editedImages.contains(path))
        return editedImages.value(path);

    if (imageCache.contains(path))
        return imageCache.value(path);

    if (path.startsWith(QStringLiteral("demo://"))) {
        bool valid = false;
        const int index = path.mid(QStringLiteral("demo://").size()).toInt(&valid);
        const QList<QImage> &demos = createDemoPhotos();
        if (valid && index >= 1 && index <= demos.size())
            return demos.at(index - 1);
        return QImage();
    }

    return QImage();
}

QImage MainWindow::decodeImage(const QString &path) const
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}

QImage MainWindow::thumbnailForPath(const QString &path, const QSize &size)
{
    const QString key = path + QStringLiteral("@%1x%2").arg(size.width()).arg(size.height());
    const QImage cached = thumbnailCache.value(key);
    if (!cached.isNull())
        return cached;

    QImage source;
    if (editedImages.contains(path)) {
        source = editedImages.value(path);
    } else if (path.startsWith(QStringLiteral("demo://"))) {
        source = imageForPath(path);
    } else {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize orig = reader.size();
        QSize scaledSize = size * 2;
        if (orig.isValid() && orig.width() > 0 && orig.height() > 0)
            scaledSize = orig.scaled(size * 2, Qt::KeepAspectRatio);
        reader.setScaledSize(scaledSize);
        source = reader.read();
    }

    if (source.isNull())
        return QImage();

    QImage thumb = source.scaled(size, Qt::KeepAspectRatioByExpanding,
                                 Qt::FastTransformation);
    qInfo().noquote() << "THUMB" << path << "source" << source.size()
                      << "scaled" << thumb.size() << "cell" << size;
    if (thumb.width() > size.width() || thumb.height() > size.height()) {
        thumb = thumb.copy((thumb.width() - size.width()) / 2,
                           (thumb.height() - size.height()) / 2,
                           size.width(), size.height());
    }
    if (thumbnailCache.size() < 96)
        thumbnailCache.insert(key, thumb);
    return thumb;
}

const QList<QImage> &MainWindow::createDemoPhotos()
{
    static const QList<QImage> demos = []() {
        QList<QImage> photos;
        const QList<QPair<QColor, QColor> > colors = QList<QPair<QColor, QColor> >()
                << qMakePair(QColor(17, 80, 160), QColor(230, 70, 140))
                << qMakePair(QColor(20, 140, 110), QColor(220, 190, 70))
                << qMakePair(QColor(70, 50, 150), QColor(20, 180, 220))
                << qMakePair(QColor(100, 180, 30), QColor(60, 90, 100));

        for (int index = 0; index < 15; ++index) {
            QImage image(810, 540, QImage::Format_RGB32);
            QPainter painter(&image);
            QLinearGradient gradient(0, 0, image.width(), image.height());
            gradient.setColorAt(0.0, colors.at(index % colors.size()).first);
            gradient.setColorAt(1.0, colors.at(index % colors.size()).second);
            painter.fillRect(image.rect(), gradient);
            painter.setPen(QPen(QColor(255, 255, 255, 190), 8));
            painter.drawEllipse(image.rect().adjusted(100, 80, -100, -80));
            painter.setPen(QColor(255, 255, 255, 235));
            QFont font = painter.font();
            font.setPixelSize(72);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(image.rect(), Qt::AlignCenter,
                             QStringLiteral("Demo %1").arg(index + 1));
            photos.append(image);
        }
        return photos;
    }();
    return demos;
}

void MainWindow::showPrevious()
{
    if (photoPaths.isEmpty())
        return;
    --currentIndex;
    showPhoto();
}

void MainWindow::showNext()
{
    if (photoPaths.isEmpty())
        return;
    ++currentIndex;
    showPhoto();
}

void MainWindow::startCrop()
{
    if (!photoView->image().isNull())
        photoView->setViewMode(PhotoView::CropMode);
    updateActions();
}

void MainWindow::applyCrop()
{
    const QImage cropped = photoView->selectedImage();
    if (cropped.isNull())
        return;
    editedImages.insert(photoPaths.at(currentIndex), cropped);
    photoView->setViewMode(PhotoView::BrowseMode);
    photoView->setImage(cropped);
    updateStatus();
    updateActions();
}

void MainWindow::cancelCrop()
{
    photoView->setViewMode(PhotoView::BrowseMode);
    updateActions();
}

void MainWindow::resetPhoto()
{
    editedImages.remove(photoPaths.value(currentIndex));
    showPhoto();
}

void MainWindow::deletePhoto()
{
    if (photoPaths.isEmpty())
        return;

    const QString path = photoPaths.at(currentIndex);
    editedImages.remove(path);
    imageCache.remove(path);
    imageCacheOrder.removeAll(path);
    foreach (const QString &key, thumbnailCache.keys()) {
        if (key.startsWith(path + QStringLiteral("@")))
            thumbnailCache.remove(key);
    }
    if (!path.startsWith(QStringLiteral("demo://")))
        QFile::remove(path);
    photoPaths.removeAt(currentIndex);
    if (photoPaths.isEmpty()) {
        for (int index = 1; index <= 15; ++index)
            photoPaths.append(QStringLiteral("demo://%1").arg(index));
        currentIndex = 0;
    } else {
        currentIndex = qMin(currentIndex, photoPaths.size() - 1);
    }
    showPhoto();
}

void MainWindow::savePhoto()
{
    const QImage image = photoView->image();
    if (image.isNull())
        return;

    const QString source = photoPaths.at(currentIndex);
    QString exportDirectory = QStringLiteral(".");
    if (!source.startsWith(QStringLiteral("demo://")))
        exportDirectory = QFileInfo(source).absolutePath() + QStringLiteral("/cropped");
    QDir().mkpath(exportDirectory);
    const QString fileName = exportDirectory + QStringLiteral("/album-%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));
    if (!image.save(fileName, "PNG")) {
        statusLabel->setText(tr("保存失败"));
        return;
    }

    statusLabel->setText(tr("已保存 %1").arg(fileName));
    if (!photoPaths.contains(fileName)) {
        photoPaths.append(fileName);
        currentIndex = photoPaths.size() - 1;
        cacheImage(fileName, image);
        photoView->setImage(image);
        updateStatus();
    }
}
