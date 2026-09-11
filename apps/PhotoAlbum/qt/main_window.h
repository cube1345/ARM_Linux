#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QMainWindow>

class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QSlider;
class PhotoView;
class QGridLayout;
class QPropertyAnimation;
class QScrollArea;
class QWidget;
class ThumbDragScroll;
class VideoPlayerWidget;

/**
 * @brief 相册浏览与编辑主窗口。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 创建相册主窗口。
     * @param photoDirectory 待读取图片所在目录。
     * @param parent Qt 父控件，可选。
     */
    explicit MainWindow(const QString &photoDirectory, QWidget *parent = nullptr);

private slots:
    void showPrevious();
    void showNext();
    void startCrop();
    void applyCrop();
    void cancelCrop();
    void resetPhoto();
    void savePhoto();
    void deletePhoto();
    void showThumbnailPage();
    void showDetailPage(int index);
    void selectFilmstripPhoto();
    /** @brief 根据上滑/下滑展开或收起底部横向缩略图列表。 */
    void setFilmstripVisible(bool visible);
    /** @brief 显示桌面首页。 */
    void showHomePage();
    /** @brief 显示监控九宫格。 */
    void showMonitorPage();
    /** @brief 刷新九宫格快照。 */
    void refreshMonitorSnapshots();
    void startMonitorMjpeg();
    void stopMonitorMjpeg();
    QString gridServerBase() const;
    /** @brief 打开某路摄像头的单路实时画面。 */
    void openMonitorChannel(int index);
    /** @brief 大图异步解码完成后更新当前图片。 */
    void onImageLoaded();
    /** @brief 进入视频列表页。 */
    void showVideoListPage();
    /** @brief 播放列表中的某一路视频。 */
    void openVideo(int row);
    /** @brief 返回视频列表页并停止播放。 */
    void videoBack();
    /** @brief 切换播放/暂停。 */
    void toggleVideoPlay();
    void toggleVideoMute();
    /** @brief 拖动进度条跳转。 */
    void onVideoSeek(int ms);
    /** @brief 回放位置变化更新进度条。 */
    void onVideoPosition(qint64 ms);
    /** @brief 回放总时长设置进度条范围。 */
    void onVideoDuration(qint64 ms);
    /** @brief 删除当前录像文件。 */
    void deleteRecording();
    /**
     * @brief 更新界面上的触摸调试标签。
     * @param contactCount 当前活动接触点数量。
     * @param sliding 接触点是否超过滑动判定阈值。
     * @param movement 最大移动距离，单位为像素。
     */
    void updateTouchDebug(int contactCount, bool sliding, qreal movement);

private:
    void buildUi();
    void buildThumbnailPage(QWidget *page);
    void buildFilmstrip(QWidget *parent);
void buildFilmstripContent();
    void buildHomePage(QWidget *page);
    void buildMonitorPage(QWidget *page);
    void rebuildThumbnailGrid();
    void buildVideoListPage(QWidget *page);
    void buildVideoPage(QWidget *page);
    QWidget *makeAppCell(const QString &name, const QString &appId,
                         const QPixmap &icon, const char *slot);
    QPixmap makeAlbumIcon() const;
    QPixmap makeVideoIcon() const;
    QPixmap makeMonitorIcon() const;
    QString monitorLiveBase() const;
    QString monitorServerBase() const;
    void loadPhotos(const QString &directory);
    void showPhoto();
    void updateActions();
    void updateStatus();
    QStringList findVideos() const;
    QStringList findStreams() const;
    void stopVideo();
    QString streamServerBase() const;
    void loadRecordings();
    void cacheImage(const QString &path, const QImage &image);
    QImage thumbnailForPath(const QString &path, const QSize &size);
    QImage decodeImage(const QString &path) const;
    QImage imageForPath(const QString &path) const;
    static const QList<QImage> &createDemoPhotos();

    QWidget *stackedWidget;
    QWidget *homePage;
    QWidget *monitorPage;
    QWidget *thumbnailPage;
    QWidget *detailPage;
    QWidget *videoListPage;
    QWidget *videoPage;
    QWidget *videoReturnPage;
    QList<QPushButton *> monitorCells;
    QTimer *monitorRefreshTimer;
    QList<QNetworkReply *> monitorReplies;
    QList<QNetworkReply *> monitorMjpegReplies;
    QList<QByteArray> monitorMjpegBuffers;
    QWidget *filmstripContainer;
    QScrollArea *filmstripScrollArea;
    QPropertyAnimation *filmstripAnimation;
    bool filmstripVisible;
    QWidget *footerBar;
    QPropertyAnimation *footerAnimation;
    ThumbDragScroll *gridDragScroll;
    ThumbDragScroll *filmstripDragScroll;
    QGridLayout *thumbnailGrid;
    PhotoView *photoView;
    QListWidget *videoList;
    VideoPlayerWidget *videoPlayer;
    QLabel *videoTitle;
    QPushButton *videoPlayButton;
    QPushButton *videoMuteButton;
    QSlider *videoSeekSlider;
    QLabel *videoTimeLabel;
    QPushButton *videoDeleteButton;
    QNetworkAccessManager *networkManager;
    bool sliderDragging;
    QString recordingStartText;
    QPushButton *backButton;
    QLabel *titleLabel;
    QLabel *statusLabel;
    QLabel *touchCountLabel;
    QLabel *touchSlideLabel;
    QPushButton *cropButton;
    QPushButton *deleteButton;
    QPushButton *editButton;
    QPushButton *applyButton;
    QPushButton *cancelButton;
    QPushButton *resetButton;
    QPushButton *saveButton;

    QFutureWatcher<QImage> *loadWatcher;
    QString loadingPath;
    int loadGeneration;
    QStringList photoPaths;
    QHash<QString, QImage> editedImages;
    QHash<QString, QImage> thumbnailCache;
    QHash<QString, QImage> imageCache;
    QStringList imageCacheOrder;
    QString photoDirectory;
    int currentIndex;
};
#endif
