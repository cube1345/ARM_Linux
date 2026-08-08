#include "main_window.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPixmap>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &mediaRoot, QWidget *parent)
    : QMainWindow(parent), mediaRoot_(QDir::cleanPath(mediaRoot))
{
    resize(1024, 680);
    setWindowTitle(tr("Media Browser"));
    buildLayout();
    refreshFiles();
    refreshGallery();
}

void MainWindow::buildLayout()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    auto *sidebar = new QVBoxLayout;
    auto *title = new QLabel(tr("MEDIA\nBROWSER"));
    title->setObjectName("title");
    sidebar->addWidget(title);
    const QList<QPair<QString, void (MainWindow::*)()>> actions = {
        {tr("▦  Gallery"), &MainWindow::showGallery},
        {tr("☷  Files"), &MainWindow::showFiles},
        {tr("♫  Audio"), &MainWindow::showFiles},
        {tr("▶  Video"), &MainWindow::showFiles},
        {tr("Aa  Text"), &MainWindow::showText},
        {tr("⚙  Settings"), &MainWindow::showSettings}
    };
    for (const auto &action : actions) {
        auto *button = new QPushButton(action.first);
        connect(button, &QPushButton::clicked, this, action.second);
        sidebar->addWidget(button);
    }
    sidebar->addStretch();
    auto *version = new QLabel(tr("Qt 5.15 • LinuxFB"));
    version->setObjectName("muted");
    sidebar->addWidget(version);
    auto *panel = new QWidget;
    panel->setLayout(sidebar);
    panel->setFixedWidth(190);
    rootLayout->addWidget(panel);

    pages_ = new QStackedWidget;
    auto *galleryPage = new QWidget;
    auto *galleryLayout = new QVBoxLayout(galleryPage);
    auto *galleryTitle = new QLabel(tr("Gallery"));
    galleryTitle->setObjectName("title");
    galleryLayout->addWidget(galleryTitle);
    gallery_ = new QListWidget;
    gallery_->setViewMode(QListView::IconMode);
    gallery_->setIconSize(QSize(180, 130));
    gallery_->setResizeMode(QListView::Adjust);
    gallery_->setSpacing(14);
    connect(gallery_, &QListWidget::itemDoubleClicked, this,
            &MainWindow::openSelectedFile);
    galleryLayout->addWidget(gallery_);
    pages_->addWidget(galleryPage);

    auto *filesPage = new QWidget;
    auto *filesLayout = new QVBoxLayout(filesPage);
    locationLabel_ = new QLabel;
    locationLabel_->setObjectName("muted");
    filesLayout->addWidget(locationLabel_);
    fileList_ = new QListWidget;
    connect(fileList_, &QListWidget::itemDoubleClicked, this,
            &MainWindow::openSelectedFile);
    filesLayout->addWidget(fileList_);
    pages_->addWidget(filesPage);

    textView_ = new QTextEdit;
    textView_->setReadOnly(true);
    pages_->addWidget(textView_);

    auto *settings = new QLabel(tr("Settings\n\nQt UI is enabled. Runtime media and input settings remain shared with the C backend."));
    settings->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    settings->setMargin(24);
    pages_->addWidget(settings);
    rootLayout->addWidget(pages_, 1);
    setCentralWidget(central);
    statusBar()->showMessage(tr("Ready — %1").arg(mediaRoot_));
}

void MainWindow::refreshFiles()
{
    QDir directory(mediaRoot_);
    directory.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    directory.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    fileList_->clear();
    for (const QFileInfo &info : directory.entryInfoList()) {
        auto *item = new QListWidgetItem(info.isDir() ? QStringLiteral("📁 ") : QStringLiteral("• ") + info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());
        fileList_->addItem(item);
    }
    locationLabel_->setText(tr("Files  •  %1").arg(mediaRoot_));
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
    } else {
        statusBar()->showMessage(tr("Media backend will open: %1").arg(path));
    }
}

void MainWindow::loadImageFile(const QString &path)
{
    QPixmap pixmap(path);
    imageView_ = new QLabel;
    imageView_->setAlignment(Qt::AlignCenter);
    imageView_->setPixmap(pixmap.scaled(900, 560, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    pages_->addWidget(imageView_);
    pages_->setCurrentWidget(imageView_);
}

void MainWindow::loadTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    textView_->setPlainText(QString::fromUtf8(file.readAll()));
    pages_->setCurrentWidget(textView_);
}

void MainWindow::showGallery() { pages_->setCurrentIndex(0); }
void MainWindow::showFiles() { pages_->setCurrentIndex(1); }
void MainWindow::showText() { pages_->setCurrentIndex(2); }
void MainWindow::showSettings() { pages_->setCurrentIndex(3); }
