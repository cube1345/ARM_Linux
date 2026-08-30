#ifndef MEDIA_BROWSER_QT_MAIN_WINDOW_H
#define MEDIA_BROWSER_QT_MAIN_WINDOW_H

#include <QMainWindow>

class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QLabel;
class QTextEdit;

/** @brief Qt Widgets desktop shell for the media browser. */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief Create a browser window rooted at the supplied media directory. */
    explicit MainWindow(const QString &mediaRoot, QWidget *parent = nullptr);

private slots:
    void showDesktop();
    void openSelectedFile(QListWidgetItem *item);
    void showGallery();
    void showFiles();
    void showAudio();
    void showVideo();
    void showText();
    void showSettings();

private:
    void buildLayout();
    void refreshFiles();
    void refreshGallery();
    void loadTextFile(const QString &path);
    void loadImageFile(const QString &path);
    bool isImage(const QString &path) const;
    bool isText(const QString &path) const;
    bool matchesFilter(const QString &path) const;

    enum class FileFilter { All, Audio, Video, Text };

    QString mediaRoot_;
    QStackedWidget *pages_ = nullptr;
    QListWidget *fileList_ = nullptr;
    QListWidget *gallery_ = nullptr;
    QLabel *imageView_ = nullptr;
    QTextEdit *textView_ = nullptr;
    QLabel *locationLabel_ = nullptr;
    FileFilter fileFilter_ = FileFilter::All;
};

#endif
