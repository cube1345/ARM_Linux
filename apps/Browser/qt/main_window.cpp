#include "main_window.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPixmap>
#include <QFrame>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTimer>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
const int kNavCount = 6;
const int kImagePreviewMaxW = 620;
const int kImagePreviewMaxH = 420;
}

MainWindow::MainWindow(const QString &mediaRoot, QWidget *parent)
    : QMainWindow(parent), mediaRoot_(QDir::cleanPath(mediaRoot))
{
    audioInitialized_ = audio_player_init(&audioPlayer_) == 0;
    videoInitialized_ = media_player_init(&videoPlayer_) == 0;
    resize(800, 480);
    setWindowTitle(tr("Media Browser"));
    buildLayout();
    refreshFiles();
    refreshGallery();
    audioTimer_ = new QTimer(this);
    audioTimer_->setInterval(500);
    connect(audioTimer_, &QTimer::timeout, this, &MainWindow::updateAudioStatus);
    if (audioInitialized_) audioTimer_->start();
    videoTimer_ = new QTimer(this);
    videoTimer_->setInterval(33);
    connect(videoTimer_, &QTimer::timeout, this, &MainWindow::updateVideoFrame);
}

MainWindow::~MainWindow()
{
    if (audioInitialized_) audio_player_destroy(&audioPlayer_);
    if (videoInitialized_) media_player_destroy(&videoPlayer_);
}

void MainWindow::buildLayout()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    auto *sidebar = new QVBoxLayout;
    sidebar->setContentsMargins(14, 18, 14, 14);
    sidebar->setSpacing(5);
    auto *title = new QLabel(tr("MEDIA\nBROWSER"));
    title->setObjectName("title");
    sidebar->addWidget(title);
    const QList<QPair<QString, void (MainWindow::*)()>> actions = {
        {tr("▦  Gallery"), &MainWindow::showGallery},
        {tr("☷  Files"), &MainWindow::showFiles},
        {tr("♫  Audio"), &MainWindow::showAudio},
        {tr("▶  Video"), &MainWindow::showVideo},
        {tr("Aa  Text"), &MainWindow::showText},
        {tr("⚙  Settings"), &MainWindow::showSettings}
    };
    for (int i = 0; i < actions.size(); ++i) {
        auto *button = new QPushButton(actions[i].first);
        button->setObjectName("navButton");
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this, actions[i].second);
        navButtons_[i] = button;
        sidebar->addWidget(button);
    }
    sidebar->addStretch();
    auto *version = new QLabel(tr("Qt 5.15 • LinuxFB"));
    version->setObjectName("muted");
    sidebar->addWidget(version);
    auto *panel = new QFrame;
    panel->setObjectName("sidebar");
    panel->setLayout(sidebar);
    panel->setFixedWidth(158);
    rootLayout->addWidget(panel);

    pages_ = new QStackedWidget;
    auto *galleryPage = new QWidget;
    auto *galleryLayout = new QVBoxLayout(galleryPage);
    galleryLayout->setContentsMargins(18, 16, 18, 12);
    galleryLayout->setSpacing(10);
    auto *galleryTitle = new QLabel(tr("Gallery"));
    galleryTitle->setObjectName("pageTitle");
    galleryLayout->addWidget(galleryTitle);
    gallery_ = new QListWidget;
    gallery_->setViewMode(QListView::IconMode);
    gallery_->setIconSize(QSize(120, 84));
    gallery_->setResizeMode(QListView::Adjust);
    gallery_->setSpacing(6);
    connect(gallery_, &QListWidget::itemActivated, this,
            &MainWindow::openSelectedFile);
    galleryLayout->addWidget(gallery_);
    pages_->addWidget(galleryPage);

    auto *filesPage = new QWidget;
    auto *filesLayout = new QVBoxLayout(filesPage);
    filesLayout->setContentsMargins(18, 16, 18, 12);
    filesLayout->setSpacing(8);
    locationLabel_ = new QLabel;
    locationLabel_->setObjectName("muted");
    filesLayout->addWidget(locationLabel_);
    fileList_ = new QListWidget;
    connect(fileList_, &QListWidget::itemActivated, this,
            &MainWindow::openSelectedFile);
    filesLayout->addWidget(fileList_);
    pages_->addWidget(filesPage);

    textView_ = new QTextEdit;
    textView_->setReadOnly(true);
    textView_->setContentsMargins(18, 16, 18, 12);
    pages_->addWidget(textView_);
    audioPage_ = new QWidget;
    auto *audioLayout = new QVBoxLayout(audioPage_);
    audioLayout->setContentsMargins(24, 24, 24, 24);
    audioTitle_ = new QLabel(tr("Audio"));
    audioTitle_->setObjectName("pageTitle");
    audioLayout->addWidget(audioTitle_);
    audioStatus_ = new QLabel(tr("Select a WAV or MP3 file."));
    audioStatus_->setObjectName("muted");
    audioLayout->addWidget(audioStatus_);
    auto *audioControls = new QHBoxLayout;
    auto *playButton = new QPushButton(tr("Play / Pause"));
    auto *stopButton = new QPushButton(tr("Stop"));
    connect(playButton, &QPushButton::clicked, this, &MainWindow::toggleAudio);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopAudio);
    audioControls->addWidget(playButton);
    audioControls->addWidget(stopButton);
    audioLayout->addLayout(audioControls);
    audioVolume_ = new QSlider(Qt::Horizontal);
    audioVolume_->setRange(0, 100);
    audioVolume_->setValue(70);
    connect(audioVolume_, &QSlider::valueChanged, this, &MainWindow::setAudioVolume);
    audioLayout->addWidget(new QLabel(tr("Volume")));
    audioLayout->addWidget(audioVolume_);
    audioLayout->addStretch();
    pages_->addWidget(audioPage_);

    videoPage_ = new QWidget;
    auto *videoLayout = new QVBoxLayout(videoPage_);
    videoLayout->setContentsMargins(18, 16, 18, 12);
    videoTitle_ = new QLabel(tr("Video"));
    videoTitle_->setObjectName("pageTitle");
    videoLayout->addWidget(videoTitle_);
    videoView_ = new QLabel(tr("Select a video file."));
    videoView_->setAlignment(Qt::AlignCenter);
    videoView_->setMinimumSize(640, 360);
    videoView_->setStyleSheet(QStringLiteral("background:#000;color:#ddd"));
    videoLayout->addWidget(videoView_, 1);
    videoStatus_ = new QLabel(tr("Stopped"));
    videoStatus_->setObjectName("muted");
    videoLayout->addWidget(videoStatus_);
    auto *videoControls = new QHBoxLayout;
    auto *videoPlayButton = new QPushButton(tr("Play / Pause"));
    auto *videoStopButton = new QPushButton(tr("Stop"));
    connect(videoPlayButton, &QPushButton::clicked, this, &MainWindow::toggleVideo);
    connect(videoStopButton, &QPushButton::clicked, this, &MainWindow::stopVideo);
    videoControls->addWidget(videoPlayButton);
    videoControls->addWidget(videoStopButton);
    videoLayout->addLayout(videoControls);
    pages_->addWidget(videoPage_);

    auto *settings = new QLabel(tr("Settings\n\nQt UI is enabled.\nRuntime media and input settings remain shared with the C backend."));
    settings->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    settings->setMargin(18);
    pages_->addWidget(settings);
    rootLayout->addWidget(pages_, 1);
    setCentralWidget(central);
    statusBar()->showMessage(tr("Ready — %1").arg(mediaRoot_));
    setCurrentNav(0);
}

void MainWindow::refreshFiles()
{
    QDir directory(mediaRoot_);
    directory.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    directory.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    fileList_->clear();
    for (const QFileInfo &info : directory.entryInfoList()) {
        if (!info.isDir() && !matchesFilter(info.fileName())) continue;
        auto *item = new QListWidgetItem(info.isDir() ? QStringLiteral("📁 ") : QStringLiteral("• ") + info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());
        fileList_->addItem(item);
    }
    QString category = tr("Files");
    if (fileFilter_ == FileFilter::Audio) category = tr("Audio");
    if (fileFilter_ == FileFilter::Video) category = tr("Video");
    if (fileFilter_ == FileFilter::Text) category = tr("Text");
    locationLabel_->setText(tr("%1  •  %2").arg(category, mediaRoot_));
}

void MainWindow::refreshGallery()
{
    QDir directory(mediaRoot_);
    gallery_->clear();
    const QStringList filters = {"*.bmp", "*.jpg", "*.jpeg", "*.png", "*.gif"};
    for (const QFileInfo &info : directory.entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        QPixmap pixmap(info.absoluteFilePath());
        if (pixmap.isNull()) continue;
        auto *item = new QListWidgetItem(QIcon(pixmap), info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());
        gallery_->addItem(item);
    }
}

bool MainWindow::isImage(const QString &path) const
{
    return QImageReader::imageFormat(path).isEmpty() == false;
}

bool MainWindow::isText(const QString &path) const
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "txt" || suffix == "log" || suffix == "md" || suffix == "json";
}

bool MainWindow::matchesFilter(const QString &path) const
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (fileFilter_ == FileFilter::Text) return isText(path);
    if (fileFilter_ == FileFilter::Audio) {
        return QStringList{"wav", "mp3", "aac", "m4a", "flac", "ogg", "opus"}.contains(suffix);
    }
    if (fileFilter_ == FileFilter::Video) {
        return QStringList{"mp4", "mov", "mkv", "avi", "webm", "m4v"}.contains(suffix);
    }
    return true;
}

void MainWindow::openSelectedFile(QListWidgetItem *item)
{
    if (item == nullptr) return;
    const QString path = item->data(Qt::UserRole).toString();
    const QFileInfo info(path);
    if (info.isDir()) {
        mediaRoot_ = info.absoluteFilePath();
        refreshFiles();
        refreshGallery();
        showFiles();
    } else if (isImage(path)) {
        loadImageFile(path);
    } else if (isText(path)) {
        loadTextFile(path);
    } else if (QStringList{"wav", "mp3"}.contains(info.suffix().toLower())) {
        playAudioFile(path);
    } else if (QStringList{"mp4", "mov", "mkv", "avi", "webm", "m4v"}.contains(info.suffix().toLower())) {
        playVideoFile(path);
    } else {
        statusBar()->showMessage(tr("Media backend will open: %1").arg(path));
    }
}

void MainWindow::playAudioFile(const QString &path)
{
    if (!audioInitialized_) {
        statusBar()->showMessage(tr("Audio backend is unavailable."));
        return;
    }
    const QByteArray device = qgetenv("BROWSER_ALSA_DEVICE");
    const char *alsaDevice = device.isEmpty() ? "default" : device.constData();
    if (audio_player_start(&audioPlayer_, path.toLocal8Bit().constData(), alsaDevice) < 0) {
        audioStatus_->setText(tr("Failed to start: %1").arg(path));
        pages_->setCurrentWidget(audioPage_);
        return;
    }
    audioTitle_->setText(tr("Audio: %1").arg(QFileInfo(path).fileName()));
    audioStatus_->setText(tr("Playing"));
    pages_->setCurrentWidget(audioPage_);
}

void MainWindow::toggleAudio()
{
    if (audioInitialized_) audio_player_toggle_pause(&audioPlayer_);
}

void MainWindow::stopAudio()
{
    if (audioInitialized_) audio_player_stop(&audioPlayer_);
    if (audioStatus_) audioStatus_->setText(tr("Stopped"));
}

void MainWindow::setAudioVolume(int volume)
{
    if (audioInitialized_) audio_player_set_volume(&audioPlayer_, volume);
}

void MainWindow::updateAudioStatus()
{
    if (!audioInitialized_ || !audioStatus_) return;
    struct audio_player_status status;
    audio_player_get_status(&audioPlayer_, &status);
    audioStatus_->setText(tr("%1  %2/%3 s  volume %4%")
                          .arg(QString::fromLatin1(audio_player_state_name(status.state)))
                          .arg(status.position_ms / 1000)
                          .arg(status.duration_ms / 1000)
                          .arg(status.volume));
}
void MainWindow::playVideoFile(const QString &path)
{
    if (!videoInitialized_) {
        statusBar()->showMessage(tr("Video backend is unavailable."));
        return;
    }
    const QByteArray device = qgetenv("BROWSER_ALSA_DEVICE");
    const char *alsaDevice = device.isEmpty() ? "default" : device.constData();
    if (media_player_start(&videoPlayer_, path.toLocal8Bit().constData(), alsaDevice) < 0) {
        videoStatus_->setText(tr("Failed to start: %1").arg(path));
        pages_->setCurrentWidget(videoPage_);
        return;
    }
    videoFrameSerial_ = 0;
    videoTitle_->setText(tr("Video: %1").arg(QFileInfo(path).fileName()));
    videoStatus_->setText(tr("Starting"));
    pages_->setCurrentWidget(videoPage_);
    if (videoTimer_) videoTimer_->start();
}

void MainWindow::toggleVideo()
{
    if (videoInitialized_) media_player_toggle_pause(&videoPlayer_);
}

void MainWindow::stopVideo()
{
    if (videoInitialized_) media_player_stop(&videoPlayer_);
    if (videoTimer_) videoTimer_->stop();
    if (videoStatus_) videoStatus_->setText(tr("Stopped"));
}

void MainWindow::updateVideoFrame()
{
    if (!videoInitialized_) return;
    struct image_data frame = {};
    uint64_t serial = 0;
    if (media_player_copy_frame(&videoPlayer_, &frame, &serial) && serial != videoFrameSerial_) {
        const QImage image(frame.pixels, static_cast<int>(frame.width),
                           static_cast<int>(frame.height),
                           static_cast<int>(frame.line_length), QImage::Format_RGB888);
        videoView_->setPixmap(QPixmap::fromImage(image.copy()).scaled(
            videoView_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        videoFrameSerial_ = serial;
    }
    struct media_player_status status = {};
    media_player_get_status(&videoPlayer_, &status);
    videoStatus_->setText(tr("%1  %2/%3 s  %4x%5  decoder=%6")
                          .arg(QString::fromLatin1(media_player_state_name(status.state)))
                          .arg(status.position_ms / 1000)
                          .arg(status.duration_ms / 1000)
                          .arg(status.width).arg(status.height)
                          .arg(QString::fromLatin1(status.video_decoder)));
    image_data_destroy(&frame);
    if (status.state == MEDIA_PLAYER_ENDED || status.state == MEDIA_PLAYER_ERROR) {
        videoTimer_->stop();
    }
}

void MainWindow::loadImageFile(const QString &path)
{
    QPixmap pixmap(path);
    if (pixmap.isNull()) return;
    imageView_ = new QLabel;
    imageView_->setAlignment(Qt::AlignCenter);
    imageView_->setPixmap(pixmap.scaled(kImagePreviewMaxW, kImagePreviewMaxH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    pages_->addWidget(imageView_);
    pages_->setCurrentWidget(imageView_);
    imagePaths_.clear();
    QDir directory(mediaRoot_);
    const QStringList filters = {"*.bmp", "*.jpg", "*.jpeg", "*.png", "*.gif"};
    for (const QFileInfo &info : directory.entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        imagePaths_.append(info.absoluteFilePath());
    }
    imageIndex_ = imagePaths_.indexOf(path);
    setFocus();
}

void MainWindow::loadTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    textView_->setPlainText(QString::fromUtf8(file.readAll()));
    pages_->setCurrentWidget(textView_);
    setFocus();
}

void MainWindow::updateNavHighlight()
{
    for (int i = 0; i < kNavCount; ++i) {
        if (!navButtons_[i]) continue;
        navButtons_[i]->setProperty("active", i == currentNav_);
        navButtons_[i]->style()->unpolish(navButtons_[i]);
        navButtons_[i]->style()->polish(navButtons_[i]);
    }
}

void MainWindow::setCurrentNav(int index)
{
    currentNav_ = (index + kNavCount) % kNavCount;
    updateNavHighlight();
}

void MainWindow::activateCurrentNav()
{
    if (navButtons_[currentNav_]) navButtons_[currentNav_]->click();
}

void MainWindow::showGallery() { setCurrentNav(0); pages_->setCurrentIndex(0); }
void MainWindow::showFiles()
{
    setCurrentNav(1);
    fileFilter_ = FileFilter::All;
    refreshFiles();
    pages_->setCurrentIndex(1);
}
void MainWindow::showAudio()
{
    setCurrentNav(2);
    fileFilter_ = FileFilter::Audio;
    refreshFiles();
    pages_->setCurrentWidget(audioPage_);
}
void MainWindow::showVideo()
{
    setCurrentNav(3);
    fileFilter_ = FileFilter::Video;
    refreshFiles();
    pages_->setCurrentWidget(videoPage_);
}
void MainWindow::showText()
{
    setCurrentNav(4);
    fileFilter_ = FileFilter::Text;
    refreshFiles();
    pages_->setCurrentIndex(1);
}
void MainWindow::showSettings() { setCurrentNav(5); pages_->setCurrentIndex(5); }

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const int page = pages_->currentIndex();
    const bool imagePreview = (imageView_ != nullptr && pages_->currentWidget() == imageView_);

    switch (event->key()) {
    case Qt::Key_Q:
        if (event->modifiers() & Qt::ControlModifier) {
            close();
            return;
        }
        break;
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
        setCurrentNav(event->key() - Qt::Key_1);
        activateCurrentNav();
        return;
    case Qt::Key_Down:
        if (page == 0) { setCurrentNav(currentNav_ + 1); return; }
        if (page == 1 && fileList_->currentItem()) {
            const int row = fileList_->row(fileList_->currentItem());
            if (row + 1 < fileList_->count()) {
                fileList_->setCurrentRow(row + 1);
                fileList_->scrollToItem(fileList_->currentItem());
            }
            return;
        }
        break;
    case Qt::Key_Up:
        if (page == 0) { setCurrentNav(currentNav_ - 1); return; }
        if (page == 1 && fileList_->currentItem()) {
            const int row = fileList_->row(fileList_->currentItem());
            if (row > 0) {
                fileList_->setCurrentRow(row - 1);
                fileList_->scrollToItem(fileList_->currentItem());
            }
            return;
        }
        break;
    case Qt::Key_Right:
        if (page == 0) { setCurrentNav(currentNav_ + 1); return; }
        if (imagePreview && imageIndex_ >= 0) {
            imageIndex_ = (imageIndex_ + 1) % imagePaths_.size();
            loadImageFile(imagePaths_.at(imageIndex_));
            return;
        }
        break;
    case Qt::Key_Left:
        if (page == 0) { setCurrentNav(currentNav_ - 1); return; }
        if (imagePreview && imageIndex_ >= 0) {
            imageIndex_ = (imageIndex_ - 1 + imagePaths_.size()) % imagePaths_.size();
            loadImageFile(imagePaths_.at(imageIndex_));
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (page == 0) { activateCurrentNav(); return; }
        if (page == 1 && fileList_->currentItem()) {
            openSelectedFile(fileList_->currentItem());
            return;
        }
        break;
    case Qt::Key_Escape:
    case Qt::Key_Backspace:
        if (imagePreview) {
            pages_->removeWidget(imageView_);
            imageView_->deleteLater();
            imageView_ = nullptr;
            imageIndex_ = -1;
            imagePaths_.clear();
            showFiles();
            return;
        }
        if (page == 2 || page == 3) { stopAudio(); showFiles(); return; }
        if (page == 4) { stopVideo(); showFiles(); return; }
        if (page == 1) {
            QDir dir(mediaRoot_);
            if (dir.cdUp()) {
                mediaRoot_ = dir.absolutePath();
                refreshFiles();
                refreshGallery();
            } else {
                showGallery();
            }
            return;
        }
        break;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}