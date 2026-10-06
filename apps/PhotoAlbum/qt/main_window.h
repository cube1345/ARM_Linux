#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QTouchEvent>
#include <QVector3D>

#include "icm20608.h"
#include "ap3216c.h"

class QColor;
class HomePageView;
class QGraphicsProxyWidget;
class QGraphicsScene;
class QGraphicsView;
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
class BrickGame;
class Game2048;
class MusicPlayer;
class Calculator;
class DrawBoard;
class DebugPage;
class DebugOverlay;
class LabyrinthGame;
class Weather;
class SnakeGame;
class TankGame;
class TetrisGame;
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
    void onImuTick();
    void startCrop();
    void applyCrop();
    void cancelCrop();
    void resetPhoto();
    void savePhoto();
    void deletePhoto();
    void showThumbnailPage();
    void showDetailPage(int index);
    /** @brief 单指垂直滑动：上滑展示拍摄信息，下滑展示操作栏。 */
    void onVerticalSwipe(bool upward);
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
    /** @brief 进入坦克大战选关页。 */
    void showLevelSelectPage();
    /** @brief 进入坦克大战游戏。 */
    void showGamePage(int level);
    void showSnakePage();
    void showTetrisPage();
    void showBrickPage();
    void showGame2048Page();
    void showMusicPage();
    void showSettingsPage();
    void showDebugPage();
    void showLabyrinthPage();
    void showCalculatorPage();
    void showDrawPage();
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
    bool eventFilter(QObject *obj, QEvent *event) override;
    void installGlobalGestures();             // 给堆叠页递归装过滤器，实现全屏滑动
    void handleGameSwipe(int dx, int dy);     // 当前页为小游戏时按方向分发
    bool forwardTouchToPhotoView(QEvent *event); // 相册页穿透 QGraphicsView：触摸直达 photo_view
    QPointF mapToPhotoView(QEvent *event);   // viewport 触摸坐标 -> photo_view 坐标（取首个触点）
    QPointF mapTouchPointToPhotoView(const QTouchEvent::TouchPoint &tp);
    bool photoTouchOwned = false;  // 手势归属：TouchBegin 命中 photo_view 后，后续 Update/End 持续注入
    int m_gestureX;
    int m_gestureY;
    bool m_gestureTracking = false;
    void buildUi();
    void buildThumbnailPage(QWidget *page);
    void buildHomePage(QWidget *page);
    void buildMonitorPage(QWidget *page);
    void rebuildThumbnailGrid();
    void buildVideoListPage(QWidget *page);
    void buildVideoPage(QWidget *page);
    void buildGamePage(QWidget *page);
    void buildLevelSelectPage(QWidget *page);
    void buildSnakePage(QWidget *page);
    void buildTetrisPage(QWidget *page);
    void buildBrickPage(QWidget *page);
    void buildGame2048Page(QWidget *page);
    void buildLabyrinthPage(QWidget *page);
    void buildMusicPage(QWidget *page);
    void buildSettingsPage(QWidget *page);
    void buildDebugPage(QWidget *page);
    void buildCalculatorPage(QWidget *page);
    void buildDrawPage(QWidget *page);
    QWidget *makeAppCell(const QString &name, const QString &appId,
                         const QColor &tint, const QString &svgFile,
                         const QPixmap &fallbackIcon, const char *slot);
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
    void preloadAdjacent();
    QImage thumbnailForPath(const QString &path, const QSize &size);
    QImage decodeImage(const QString &path) const;
    QImage imageForPath(const QString &path) const;
    static const QList<QImage> &createDemoPhotos();
    void applyOrientation(int orient);

    QWidget *stackedWidget;
    QWidget *homePage;
    HomePageView *homeView = nullptr;
    QWidget *monitorPage;
    QWidget *thumbnailPage;
    QWidget *detailPage;
    QWidget *videoListPage;
    QWidget *videoPage;
    QWidget *videoReturnPage;
    QWidget *gamePage;
    QWidget *levelSelectPage;
    TankGame *tankGame;
    QWidget *snakePage;
    QWidget *tetrisPage;
    QWidget *brickPage;
    QWidget *game2048Page;
    QWidget *musicPage;
    QListWidget *musicList = nullptr;
    QWidget *settingsPage;
    QWidget *debugPage;
    DebugPage *debugWidget;
    QWidget *labyrinthPage;
    LabyrinthGame *labyrinthGame;
    DebugOverlay *m_debugOverlay;
    SnakeGame *snakeGame;
    TetrisGame *tetrisGame;
    BrickGame *brickGame;
    Game2048 *game2048;
    MusicPlayer *musicPlayer;
    QWidget *calculatorPage;
    QWidget *drawPage;
    Calculator *calculator;
    DrawBoard *drawBoard;
    Weather *weather;
    Ap3216c m_als;
    QLabel *weatherLabel;
    QLabel *alsLabel;
    QList<QPushButton *> monitorCells;
    QTimer *monitorRefreshTimer;
    QList<QNetworkReply *> monitorReplies;
    QList<QNetworkReply *> monitorMjpegReplies;
    QList<QByteArray> monitorMjpegBuffers;
    QWidget *footerBar;
    QLabel *infoPanel;
    Icm20608 m_imu;
    QTimer *m_imuTimer;
    int m_orientation;
    bool m_orientationLocked;
    bool m_savedOrientationLocked = false;   // 迷宫页临时锁定方向前的原状态
    qint64 m_lastShake;
    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsProxyWidget *m_proxy;
    ThumbDragScroll *gridDragScroll;
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
