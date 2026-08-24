#ifndef MEDIA_BROWSER_QT_MAIN_WINDOW_H
#define MEDIA_BROWSER_QT_MAIN_WINDOW_H

#include <QMainWindow>

extern "C" {
#include "../media/audio/audio_player.h"
#include "../media/video/media_player.h"
}

class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QLabel;
class QPushButton;
class QTextEdit;
class QTimer;
class QSlider;

/** @brief Qt Widgets desktop shell for the media browser. */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &mediaRoot, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void openSelectedFile(QListWidgetItem *item);
    void showGallery();
    void showFiles();
    void showAudio();
    void showVideo();
    void showText();
    void showSettings();
    void toggleAudio();
    void stopAudio();
    void setAudioVolume(int volume);
    void updateAudioStatus();
    void toggleVideo();
    void stopVideo();
    void updateVideoFrame();

private:
    void buildLayout();
    void refreshFiles();
    void refreshGallery();
    void loadTextFile(const QString &path);
    void loadImageFile(const QString &path);
    void playAudioFile(const QString &path);
    void playVideoFile(const QString &path);
    bool isImage(const QString &path) const;
    bool isText(const QString &path) const;
    bool matchesFilter(const QString &path) const;
    void updateNavHighlight();
    void setCurrentNav(int index);
    void activateCurrentNav();

    enum class FileFilter { All, Audio, Video, Text };

    QString mediaRoot_;
    QStackedWidget *pages_ = nullptr;
    QListWidget *fileList_ = nullptr;
    QListWidget *gallery_ = nullptr;
    QLabel *imageView_ = nullptr;
    QTextEdit *textView_ = nullptr;
    QWidget *audioPage_ = nullptr;
    QLabel *audioTitle_ = nullptr;
    QLabel *audioStatus_ = nullptr;
    QSlider *audioVolume_ = nullptr;
    QTimer *audioTimer_ = nullptr;
    QWidget *videoPage_ = nullptr;
    QLabel *videoTitle_ = nullptr;
    QLabel *videoStatus_ = nullptr;
    QLabel *videoView_ = nullptr;
    QTimer *videoTimer_ = nullptr;
    QLabel *locationLabel_ = nullptr;
    QPushButton *navButtons_[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    int currentNav_ = 0;
    QStringList imagePaths_;
    int imageIndex_ = -1;
    FileFilter fileFilter_ = FileFilter::All;
    struct audio_player audioPlayer_ {};
    bool audioInitialized_ = false;
    struct media_player videoPlayer_ {};
    bool videoInitialized_ = false;
    uint64_t videoFrameSerial_ = 0;
};

#endif
