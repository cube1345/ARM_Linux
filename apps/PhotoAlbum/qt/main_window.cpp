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
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextStream>
#include <QImageReader>
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
#include <QToolButton>
#include <QSlider>
#include <QTime>
#include <QVariant>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "photo_view.h"
#include "video_player.h"

/**
 * @brief 让缩略图区域支持按住拖动滚动：从图片按钮上起手也能滚动，
 *        轻点仍正常触发按钮点击。
 */
class ThumbDragScroll : public QObject
{
public:
    explicit ThumbDragScroll(QScrollArea *area, QObject *parent = nullptr)
        : QObject(parent), area(area), pressing(false), active(false)
    {
        area->viewport()->installEventFilter(this);
    }

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
            if (QScrollBar *verticalBar = area->verticalScrollBar())
                if (verticalBar->isVisible() && verticalBar->maximum() > 0)
                    verticalBar->setValue(verticalBar->value() - delta.y());
            if (QScrollBar *horizontalBar = area->horizontalScrollBar())
                if (horizontalBar->isVisible() && horizontalBar->maximum() > 0)
                    horizontalBar->setValue(horizontalBar->value() - delta.x());
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
        if (!m_image.isNull())
            painter.drawImage(rect(), m_image);
        else
            painter.fillRect(rect(), QColor(0x14, 0x14, 0x1a));
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
      currentIndex(0)
{
    buildUi();
    loadPhotos(photoDirectory);
}

/**
 * @brief 创建窗口布局、控件及信号槽连接。
 */
/**
 * @brief 创建缩略图页、详情页和固定底部操作栏。
 */
void MainWindow::buildUi()
{
    setWindowTitle(tr("Photo Album"));
    setMinimumSize(480, 272);

    QWidget *central = new QWidget(this);
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
    stack->addWidget(homePage);
    stack->addWidget(monitorPage);
    stack->addWidget(thumbnailPage);
    stack->addWidget(detailPage);
    stack->addWidget(videoListPage);
    stack->addWidget(videoPage);
    layout->addWidget(stack, 1);

    buildHomePage(homePage);
    buildMonitorPage(monitorPage);
    buildThumbnailPage(thumbnailPage);
    buildVideoListPage(videoListPage);
    buildVideoPage(videoPage);

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
    setCentralWidget(central);
    setStyleSheet(QStringLiteral(
        "QWidget{background:#0b0b0f;} QLabel{background:transparent;color:#f5f5f7;}"
        "QLabel#title{font-size:18px;font-weight:700;} QLabel#status,QLabel#videoTime{color:#a1a1aa;}"
        "QLabel#homeTime{font-size:52px;font-weight:200;color:#ffffff;margin-top:14px;}"
        "QLabel#appName{font-size:14px;font-weight:600;color:#ffffff;}"
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

QPixmap MainWindow::makeMonitorIcon() const
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setBrush(QColor(30, 42, 56));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    painter.setBrush(QColor(58, 131, 255));
    painter.drawRect(10, 14, 30, 22);
    painter.drawRect(48, 14, 30, 22);
    painter.drawRect(10, 52, 30, 22);
    painter.drawRect(48, 52, 30, 22);
    return icon;
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

void MainWindow::buildHomePage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

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

    layout->addStretch();
    QHBoxLayout *apps = new QHBoxLayout();
    apps->addStretch();
    apps->addWidget(makeAppCell(tr("相册"), QStringLiteral("album"),
                                makeAlbumIcon(), SLOT(showThumbnailPage())));
    apps->addSpacing(28);
    apps->addWidget(makeAppCell(tr("监控"), QStringLiteral("monitor"),
                                makeMonitorIcon(), SLOT(showMonitorPage())));
    apps->addSpacing(28);
    apps->addWidget(makeAppCell(tr("视频"), QStringLiteral("video"),
                                makeVideoIcon(), SLOT(showVideoListPage())));
    apps->addStretch();
    layout->addLayout(apps);
    layout->addStretch();
}

QWidget *MainWindow::makeAppCell(const QString &name, const QString &appId,
                                 const QPixmap &icon, const char *slot)
{
    QWidget *cell = new QWidget;
    QVBoxLayout *v = new QVBoxLayout(cell);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    QPushButton *button = new QPushButton(cell);
    button->setObjectName(QStringLiteral("appIcon"));
    button->setProperty("app", appId);
    button->setIcon(QIcon(icon));
    button->setIconSize(icon.size());
    button->setFixedSize(icon.size());
    connect(button, SIGNAL(clicked()), this, slot);
    QLabel *label = new QLabel(name, cell);
    label->setObjectName(QStringLiteral("appName"));
    label->setAlignment(Qt::AlignCenter);
    v->addWidget(button);
    v->addWidget(label);
    return cell;
}

QPixmap MainWindow::makeAlbumIcon() const
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    QLinearGradient gradient(0, 0, 88, 88);
    gradient.setColorAt(0.0, QColor(58, 131, 255));
    gradient.setColorAt(1.0, QColor(164, 90, 255));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    painter.setPen(QPen(QColor(255, 255, 255, 235), 5,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(12, 66, 34, 42);
    painter.drawLine(34, 42, 46, 58);
    painter.drawLine(46, 58, 62, 36);
    painter.drawLine(62, 36, 76, 66);
    painter.drawEllipse(QPointF(64, 24), 5, 5);
    return icon;
}

QPixmap MainWindow::makeVideoIcon() const
{
    QPixmap icon(88, 88);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setBrush(QColor(255, 59, 48));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 88, 88, 20, 20);
    QPolygon triangle;
    triangle << QPoint(34, 26) << QPoint(66, 44) << QPoint(34, 62);
    painter.setBrush(Qt::white);
    painter.drawPolygon(triangle);
    return icon;
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
    int cellW = (width() - 32) / 5;
    if (QScrollArea *scroll = qobject_cast<QScrollArea *>(content->parentWidget())) {
        const int viewport = scroll->viewport()->width();
        if (viewport >= 100)
            cellW = viewport / 5;
    }
    if (cellW < 60)
        cellW = 89;
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
        thumbnailGrid->addWidget(button, index / 5, index % 5);
    }
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
        reader.setScaledSize(size * 2);
        source = reader.read();
    }

    if (source.isNull())
        return QImage();

    QImage thumb = source.scaled(size, Qt::KeepAspectRatioByExpanding,
                                 Qt::FastTransformation);
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
