#include "main_window.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "photo_view.h"

MainWindow::MainWindow(const QString &photoDirectory, QWidget *parent)
    : QMainWindow(parent),
      photoView(nullptr),
      titleLabel(nullptr),
      statusLabel(nullptr),
      previousButton(nullptr),
      nextButton(nullptr),
      cropButton(nullptr),
      applyButton(nullptr),
      cancelButton(nullptr),
      resetButton(nullptr),
      saveButton(nullptr),
      currentIndex(0)
{
    buildUi();
    loadPhotos(photoDirectory);
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Photo Album"));
    setMinimumSize(480, 272);

    QWidget *central = new QWidget(this);
    central->setObjectName(QStringLiteral("root"));
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(4);

    QWidget *header = new QWidget(central);
    header->setObjectName(QStringLiteral("header"));
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    titleLabel = new QLabel(tr("相册"), header);
    titleLabel->setObjectName(QStringLiteral("title"));
    statusLabel = new QLabel(header);
    statusLabel->setObjectName(QStringLiteral("status"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusLabel);

    photoView = new PhotoView(central);
    connect(photoView, SIGNAL(previousRequested()),
            this, SLOT(showPrevious()));
    connect(photoView, SIGNAL(nextRequested()),
            this, SLOT(showNext()));

    QWidget *footer = new QWidget(central);
    footer->setObjectName(QStringLiteral("footer"));
    QHBoxLayout *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(5);

    previousButton = new QPushButton(QStringLiteral("‹"), footer);
    nextButton = new QPushButton(QStringLiteral("›"), footer);
    cropButton = new QPushButton(tr("裁剪"), footer);
    applyButton = new QPushButton(tr("应用"), footer);
    cancelButton = new QPushButton(tr("取消"), footer);
    resetButton = new QPushButton(tr("复位"), footer);
    saveButton = new QPushButton(tr("保存"), footer);

    previousButton->setObjectName(QStringLiteral("roundButton"));
    nextButton->setObjectName(QStringLiteral("roundButton"));
    cropButton->setObjectName(QStringLiteral("primaryButton"));
    applyButton->setObjectName(QStringLiteral("primaryButton"));
    cancelButton->setObjectName(QStringLiteral("roundButton"));
    resetButton->setObjectName(QStringLiteral("roundButton"));
    saveButton->setObjectName(QStringLiteral("roundButton"));

    connect(previousButton, SIGNAL(clicked()), this, SLOT(showPrevious()));
    connect(nextButton, SIGNAL(clicked()), this, SLOT(showNext()));
    connect(cropButton, SIGNAL(clicked()), this, SLOT(startCrop()));
    connect(applyButton, SIGNAL(clicked()), this, SLOT(applyCrop()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelCrop()));
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetPhoto()));
    connect(saveButton, SIGNAL(clicked()), this, SLOT(savePhoto()));

    footerLayout->addWidget(previousButton);
    footerLayout->addWidget(nextButton);
    footerLayout->addStretch();
    footerLayout->addWidget(cropButton);
    footerLayout->addWidget(applyButton);
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(resetButton);
    footerLayout->addWidget(saveButton);

    layout->addWidget(header);
    layout->addWidget(photoView, 1);
    layout->addWidget(footer);
    setCentralWidget(central);

    setStyleSheet(QStringLiteral(
        "QWidget#root{background:#0b0b0f;}"
        "QWidget#header,QWidget#footer{background:transparent;}"
        "QLabel{color:#f5f5f7;}"
        "QLabel#title{font-size:15px;font-weight:700;}"
        "QLabel#status{color:#a1a1aa;font-size:11px;}"
        "QPushButton{color:#f5f5f7;background:rgba(255,255,255,0.12);"
        "border:0;border-radius:12px;min-width:36px;min-height:28px;"
        "padding:0 10px;font-size:12px;}"
        "QPushButton:pressed{background:rgba(255,255,255,0.26);}"
        "QPushButton#primaryButton{background:#0a84ff;}"
        "QPushButton#primaryButton:pressed{background:#0060df;}"
    ));
}

void MainWindow::loadPhotos(const QString &directory)
{
    photoPaths.clear();
    editedImages.clear();
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
        photoPaths = QStringList() << QStringLiteral("demo://1")
                                   << QStringLiteral("demo://2")
                                   << QStringLiteral("demo://3");
    }

    showPhoto();
}

void MainWindow::showPhoto()
{
    if (photoPaths.isEmpty()) {
        photoView->setImage(QImage());
        updateStatus();
        updateActions();
        return;
    }

    currentIndex = (currentIndex + photoPaths.size()) % photoPaths.size();
    photoView->setImage(imageForPath(photoPaths.at(currentIndex)));
    photoView->setViewMode(PhotoView::BrowseMode);
    updateStatus();
    updateActions();
}

void MainWindow::updateActions()
{
    const bool cropMode = photoView->viewMode() == PhotoView::CropMode;
    cropButton->setVisible(!cropMode);
    applyButton->setVisible(cropMode);
    cancelButton->setVisible(cropMode);
    applyButton->setEnabled(photoView->hasCropSelection());
}

void MainWindow::updateStatus()
{
    if (photoPaths.isEmpty()) {
        statusLabel->setText(tr("无图片"));
        return;
    }

    const QString name = photoPaths.at(currentIndex).startsWith(QStringLiteral("demo://"))
            ? tr("演示图 %1").arg(currentIndex + 1)
            : QFileInfo(photoPaths.at(currentIndex)).fileName();
    statusLabel->setText(QStringLiteral("%1/%2  %3  %4x%5")
                         .arg(currentIndex + 1)
                         .arg(photoPaths.size())
                         .arg(name)
                         .arg(photoView->imageSize().width())
                         .arg(photoView->imageSize().height()));
}

QImage MainWindow::imageForPath(const QString &path) const
{
    if (editedImages.contains(path))
        return editedImages.value(path);

    if (path.startsWith(QStringLiteral("demo://"))) {
        bool valid = false;
        const int index = path.mid(QStringLiteral("demo://").size()).toInt(&valid);
        const QList<QImage> demos = createDemoPhotos();
        if (valid && index >= 1 && index <= demos.size())
            return demos.at(index - 1);
        return QImage();
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}

QList<QImage> MainWindow::createDemoPhotos()
{
    QList<QImage> photos;
    const QList<QPair<QColor, QColor> > colors = QList<QPair<QColor, QColor> >()
            << qMakePair(QColor(17, 80, 160), QColor(230, 70, 140))
            << qMakePair(QColor(20, 140, 110), QColor(220, 190, 70))
            << qMakePair(QColor(70, 50, 150), QColor(20, 180, 220));

    for (int index = 0; index < colors.size(); ++index) {
        QImage image(900, 600, QImage::Format_RGB32);
        QPainter painter(&image);
        QLinearGradient gradient(0, 0, image.width(), image.height());
        gradient.setColorAt(0.0, colors.at(index).first);
        gradient.setColorAt(1.0, colors.at(index).second);
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
    const QString path = photoPaths.value(currentIndex);
    editedImages.remove(path);
    photoView->setImage(imageForPath(path));
    updateStatus();
    updateActions();
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
    if (image.save(fileName, "PNG"))
        statusLabel->setText(tr("已保存 %1").arg(fileName));
    else
        statusLabel->setText(tr("保存失败"));
}
