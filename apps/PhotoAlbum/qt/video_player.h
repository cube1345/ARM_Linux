#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <QAtomicInt>
#include <QDateTime>
#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QWidget>

/**
 * @brief ffmpeg 解码线程：打开媒体文件（或网络流），软解视频帧并输出 QImage。
 */
class VideoDecodeThread : public QThread
{
    Q_OBJECT

public:
    explicit VideoDecodeThread(QObject *parent = nullptr);
    ~VideoDecodeThread() override;

    /** @brief 打开媒体并启动解码。 */
    void open(const QString &path);
    /** @brief 请求停止解码。 */
    void stop();
    /** @brief 暂停或继续解码。 */
    void setPaused(bool paused);

signals:
    /** @brief 每解码一帧输出 RGB 图像。 */
    void frameReady(const QImage &frame);
    /** @brief 播放结束或出错。error 非空表示失败原因。 */
    void ended(const QString &error);
    /** @brief 播放位置变化（毫秒，仅非直播源）。 */
    void positionChanged(qint64 ms);
    /** @brief 媒体总时长（毫秒，未知时为 0）。 */
    void durationChanged(qint64 ms);

public slots:
    /** @brief 跳转到指定毫秒位置（非直播源）。 */
    void seek(qint64 ms);

protected:
    void run() override;

private:
    QString decodeNew();
    QString decodeOld();
    QString mediaPath;
    bool streamMode;
    QAtomicInt running;
    QAtomicInt paused;
    QAtomicInt stopped;
    QAtomicInt pendingSeek;
    QAtomicInteger<qint64> pendingSeekMs;
    qint64 lastPositionEmit;
};

/**
 * @brief 视频播放控件：显示解码帧，支持暂停与返回。
 */
class VideoPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget *parent = nullptr);
    ~VideoPlayerWidget() override;

    /** @brief 打开并播放媒体。 */
    void open(const QString &path);
    /** @brief 停止播放并复位。 */
    void stop();
    /** @brief 切换暂停/继续。 */
    void togglePause();
    /** @brief 当前是否暂停。 */
    bool isPaused() const;
    /** @brief 已显示的帧数（用于调试/测试）。 */
    int frameCount() const;
    /** @brief 当前状态文本（错误/结束提示）。 */
    QString lastStatus() const;
    /** @brief 是否直播源（m3u8/rtsp/rtmp）。 */
    bool isLive() const;
    /** @brief 设置录像拍摄开始时间（用于画面叠加时间戳）。 */
    void setRecordingStart(const QDateTime &start);
    /** @brief 跳转播放位置（毫秒）。 */
    void seek(qint64 ms);
    /** @brief 当前播放位置（毫秒）。 */
    qint64 position() const;

signals:
    /** @brief 请求返回上一页。 */
    void backRequested();
    /** @brief 每秒更新一次实际显示帧率。 */
    void fpsChanged(int fps);
    /** @brief 播放位置变化（毫秒）。 */
    void positionChanged(qint64 ms);
    /** @brief 媒体总时长（毫秒，未知为 0）。 */
    void durationChanged(qint64 ms);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onFrame(const QImage &frame);
    void onEnded(const QString &error);
    void onRefresh();
    void onPositionChanged(qint64 ms);
    void onDurationChanged(qint64 ms);

private:
    VideoDecodeThread *decodeThread;
    QImage currentFrame;
    QMutex frameMutex;
    QElapsedTimer fpsTimer;
    QTimer refreshTimer;
    QDateTime recordingStart;
    bool liveSource;
    qint64 positionMs;
    qint64 durationMs;
    int fpsFrames;
    int displayFps;
    bool paused;
    int frames;
    QString statusText;
};

#endif // VIDEO_PLAYER_H