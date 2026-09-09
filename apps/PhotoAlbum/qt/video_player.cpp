#include "video_player.h"

#include <QDebug>
#include <QPainter>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#if LIBAVCODEC_VERSION_MAJOR >= 58
#define USE_NEW_DECODE_API 1
#define USE_OLD_DECODE_API 0
#else
#define USE_NEW_DECODE_API 0
#define USE_OLD_DECODE_API 1
#endif

static bool emitDecodedFrame(VideoDecodeThread *thread, AVFrame *frame,
                             AVCodecContext *codecCtx, SwsContext *&sws,
                             QByteArray &rgbBuffer)
{
    if (!sws) {
        sws = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
                             codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    }
    if (!sws)
        return false;
    const int linesize = codecCtx->width * 3;
    rgbBuffer.resize(linesize * codecCtx->height);
    uint8_t *destination[1] = { reinterpret_cast<uint8_t *>(rgbBuffer.data()) };
    int destinationLinesize[1] = { linesize };
    sws_scale(sws, frame->data, frame->linesize, 0, codecCtx->height,
              destination, destinationLinesize);
    QImage image(reinterpret_cast<const uchar *>(rgbBuffer.constData()),
                 codecCtx->width, codecCtx->height, linesize, QImage::Format_RGB888);
    emit thread->frameReady(image.copy());
    return true;
}

/**
 * @brief 按源 fps 节流：睡眠 = 目标帧间隔 - 本帧完整耗时（含解码+转格式+发射）。
 */
static void paceFrame(const QElapsedTimer &frameTimer, int fps)
{
    if (fps <= 0)
        return;
    const qint64 target = 1000 / fps;
    const qint64 elapsed = frameTimer.elapsed();
    if (elapsed < target)
        QThread::msleep(static_cast<unsigned long>(target - elapsed));
}

VideoDecodeThread::VideoDecodeThread(QObject *parent)
    : QThread(parent),
      running(0),
      paused(0),
      stopped(0),
      pendingSeek(0),
      pendingSeekMs(0),
      lastPositionEmit(-1)
{
}

VideoDecodeThread::~VideoDecodeThread()
{
    stop();
    wait();
}

void VideoDecodeThread::seek(qint64 ms)
{
    pendingSeekMs.storeRelease(ms);
    pendingSeek.storeRelease(1);
}

void VideoDecodeThread::open(const QString &path)
{
    mediaPath = path;
    const QString lower = path.toLower();
    streamMode = lower.startsWith(QStringLiteral("rtsp://"))
            || lower.startsWith(QStringLiteral("rtmp://"))
            || lower.startsWith(QStringLiteral("rtmps://"))
            || lower.contains(QStringLiteral(".m3u8"));
    start();
}

void VideoDecodeThread::stop()
{
    stopped.storeRelease(1);
    running.storeRelease(0);
}

void VideoDecodeThread::setPaused(bool value)
{
    paused.storeRelease(value ? 1 : 0);
}

void VideoDecodeThread::run()
{
    running.storeRelease(1);
    paused.storeRelease(0);
    stopped.storeRelease(0);
    QString error;
#if USE_NEW_DECODE_API
    error = decodeNew();
#else
    error = decodeOld();
#endif
    if (stopped.loadAcquire())
        emit ended(QStringLiteral("__stopped__"));
    else if (!error.isEmpty() || !running.loadAcquire())
        emit ended(error);
    else
        emit ended(QString());
    running.storeRelease(0);
}

#if USE_NEW_DECODE_API
QString VideoDecodeThread::decodeNew()
{
    AVFormatContext *format = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwsContext *sws = nullptr;
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    QByteArray rgbBuffer;
    QString error;

    avformat_network_init();

    for (;;) {
        if (codecCtx) {
            avcodec_free_context(&codecCtx);
            codecCtx = nullptr;
        }
        if (sws) {
            sws_freeContext(sws);
            sws = nullptr;
        }
        if (format) {
            avformat_close_input(&format);
            format = nullptr;
        }

        AVDictionary *options = nullptr;
        av_dict_set(&options, "rw_timeout", "2000000", 0);
        av_dict_set(&options, "max_reload", "1", 0);
        if (mediaPath.toLower().startsWith(QStringLiteral("rtsp://"))) {
            av_dict_set(&options, "rtsp_transport", "tcp", 0);
            av_dict_set(&options, "stimeout", "5000000", 0);
            av_dict_set(&options, "max_delay", "1000000", 0);
        }
        if (avformat_open_input(&format, mediaPath.toUtf8().constData(),
                                nullptr, &options) < 0) {
            av_dict_free(&options);
            if (streamMode && running.loadAcquire()) {
                qInfo().noquote() << QStringLiteral("[VideoPlayer] 流打开失败，3 秒后重连：%1")
                        .arg(mediaPath);
                    for (int i = 0; i < 30 && running.loadAcquire(); ++i)
                        msleep(100);
                    continue;
            }
            error = QStringLiteral("打开失败：%1").arg(mediaPath);
            break;
        }
        av_dict_free(&options);
        if (avformat_find_stream_info(format, nullptr) < 0) {
            error = QStringLiteral("找不到流信息");
            break;
        }
        const int videoStream = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO,
                                                    -1, -1, nullptr, 0);
        if (videoStream < 0) {
            error = QStringLiteral("没有视频流");
            break;
        }
        AVStream *stream = format->streams[videoStream];
        int fps = 25;
        if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
            fps = qBound(1, qRound(av_q2d(stream->avg_frame_rate)), 60);
        if (format->duration != AV_NOPTS_VALUE)
            emit durationChanged(format->duration * 1000 / AV_TIME_BASE);
        codecCtx = avcodec_alloc_context3(avcodec_find_decoder(stream->codecpar->codec_id));
        if (!codecCtx) {
            error = QStringLiteral("不支持该编码");
            break;
        }
        if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0 ||
                avcodec_open2(codecCtx, nullptr, nullptr) < 0) {
            error = QStringLiteral("解码器打开失败");
            break;
        }

        while (running.loadAcquire()) {
            if (paused.loadAcquire()) {
                msleep(40);
                continue;
            }
            if (pendingSeek.loadAcquire()) {
                pendingSeek.storeRelease(0);
                const qint64 targetMs = pendingSeekMs.loadAcquire();
                av_seek_frame(format, -1, targetMs * (AV_TIME_BASE / 1000),
                              AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(codecCtx);
                lastPositionEmit = -1;
            }
            const int read = av_read_frame(format, packet);
            if (read < 0) {
                if (streamMode && running.loadAcquire()) {
                    qInfo().noquote() << QStringLiteral("[VideoPlayer] 流中断，3 秒后重连：%1")
                            .arg(mediaPath);
                    for (int i = 0; i < 30 && running.loadAcquire(); ++i)
                        msleep(100);
                    break;
                }
                break;
            }
            if (packet->stream_index == videoStream) {
                avcodec_send_packet(codecCtx, packet);
                av_packet_unref(packet);
                while (running.loadAcquire() && !paused.loadAcquire()) {
                    QElapsedTimer frameTimer;
                    frameTimer.start();
                    const int receive = avcodec_receive_frame(codecCtx, frame);
                    if (receive == AVERROR(EAGAIN) || receive == AVERROR_EOF)
                        break;
                    if (receive < 0)
                        break;
                    if (!streamMode && frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                        const qint64 posMs = frame->best_effort_timestamp
                                * 1000 * av_q2d(stream->time_base);
                        if (posMs - lastPositionEmit >= 250) {
                            lastPositionEmit = posMs;
                            emit positionChanged(posMs);
                        }
                    }
                    emitDecodedFrame(this, frame, codecCtx, sws, rgbBuffer);
                    paceFrame(frameTimer, fps);
                    av_frame_unref(frame);
                }
            } else {
                av_packet_unref(packet);
            }
        }

        if (streamMode && running.loadAcquire() && error.isEmpty())
            continue;
        break;
    }

    if (sws)
        sws_freeContext(sws);
    if (codecCtx)
        avcodec_free_context(&codecCtx);
    if (format)
        avformat_close_input(&format);
    if (packet)
        av_packet_free(&packet);
    if (frame)
        av_frame_free(&frame);
    avformat_network_deinit();
    return error;
}
#else
QString VideoDecodeThread::decodeOld()
{
    av_register_all();
    avformat_network_init();
    AVFormatContext *format = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwsContext *sws = nullptr;
    AVFrame *frame = av_frame_alloc();
    QByteArray rgbBuffer;
    QString error;

    for (;;) {
        if (sws) {
            sws_freeContext(sws);
            sws = nullptr;
        }
        if (format) {
            avformat_close_input(&format);
            format = nullptr;
        }

        AVDictionary *options = nullptr;
        av_dict_set(&options, "rw_timeout", "2000000", 0);
        av_dict_set(&options, "max_reload", "1", 0);
        if (mediaPath.toLower().startsWith(QStringLiteral("rtsp://"))) {
            av_dict_set(&options, "rtsp_transport", "tcp", 0);
            av_dict_set(&options, "stimeout", "5000000", 0);
            av_dict_set(&options, "max_delay", "1000000", 0);
        }
        if (avformat_open_input(&format, mediaPath.toUtf8().constData(),
                                nullptr, &options) < 0) {
            av_dict_free(&options);
            if (streamMode && running.loadAcquire()) {
                qInfo().noquote() << QStringLiteral("[VideoPlayer] 流打开失败，3 秒后重连：%1")
                        .arg(mediaPath);
                    for (int i = 0; i < 30 && running.loadAcquire(); ++i)
                        msleep(100);
                    continue;
            }
            error = QStringLiteral("打开失败：%1").arg(mediaPath);
            break;
        }
        av_dict_free(&options);
        if (avformat_find_stream_info(format, nullptr) < 0) {
            error = QStringLiteral("找不到流信息");
            break;
        }
        const int videoStream = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO,
                                                    -1, -1, nullptr, 0);
        if (videoStream < 0) {
            error = QStringLiteral("没有视频流");
            break;
        }
        AVStream *stream = format->streams[videoStream];
        int fps = 25;
        if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
            fps = qBound(1, qRound(av_q2d(stream->avg_frame_rate)), 60);
        if (format->duration != AV_NOPTS_VALUE)
            emit durationChanged(format->duration * 1000 / AV_TIME_BASE);
        codecCtx = stream->codec;
        if (!codecCtx) {
            error = QStringLiteral("不支持该编码");
            break;
        }
        const AVCodec *decoder = avcodec_find_decoder(codecCtx->codec_id);
        if (!decoder) {
            error = QStringLiteral("找不到解码器(id=%1)").arg(codecCtx->codec_id);
            break;
        }
        codecCtx->thread_count = 1;
        const int openResult = avcodec_open2(codecCtx, decoder, nullptr);
        if (openResult < 0) {
            char errorBuffer[64];
            av_strerror(openResult, errorBuffer, sizeof(errorBuffer));
            error = QStringLiteral("解码器打开失败(%1)").arg(
                    QString::fromUtf8(errorBuffer));
            break;
        }

        while (running.loadAcquire()) {
            if (paused.loadAcquire()) {
                msleep(40);
                continue;
            }
            if (pendingSeek.loadAcquire()) {
                pendingSeek.storeRelease(0);
                const qint64 targetMs = pendingSeekMs.loadAcquire();
                av_seek_frame(format, -1, targetMs * (AV_TIME_BASE / 1000),
                              AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(codecCtx);
                lastPositionEmit = -1;
            }
            AVPacket packet;
            av_init_packet(&packet);
            const int read = av_read_frame(format, &packet);
            if (read < 0) {
                if (streamMode && running.loadAcquire()) {
                    qInfo().noquote() << QStringLiteral("[VideoPlayer] 流中断，3 秒后重连：%1")
                            .arg(mediaPath);
                    for (int i = 0; i < 30 && running.loadAcquire(); ++i)
                        msleep(100);
                    break;
                }
                break;
            }
            if (packet.stream_index == videoStream) {
                QElapsedTimer frameTimer;
                frameTimer.start();
                int gotFrame = 0;
                avcodec_decode_video2(codecCtx, frame, &gotFrame, &packet);
                av_packet_unref(&packet);
                if (gotFrame) {
                    if (!streamMode && frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                        const qint64 posMs = frame->best_effort_timestamp
                                * 1000 * av_q2d(stream->time_base);
                        if (posMs - lastPositionEmit >= 250) {
                            lastPositionEmit = posMs;
                            emit positionChanged(posMs);
                        }
                    }
                    emitDecodedFrame(this, frame, codecCtx, sws, rgbBuffer);
                    paceFrame(frameTimer, fps);
                }
            } else {
                av_packet_unref(&packet);
            }
        }

        if (streamMode && running.loadAcquire() && error.isEmpty())
            continue;
        break;
    }

    if (sws)
        sws_freeContext(sws);
    if (format)
        avformat_close_input(&format);
    av_frame_free(&frame);
    avformat_network_deinit();
    return error;
}
#endif

VideoPlayerWidget::VideoPlayerWidget(QWidget *parent)
    : QWidget(parent),
      decodeThread(new VideoDecodeThread(this)),
      liveSource(false),
      positionMs(0),
      durationMs(0),
      fpsFrames(0),
      displayFps(0),
      paused(false),
      frames(0)
{
    fpsTimer.start();
    refreshTimer.setInterval(30);
    refreshTimer.setTimerType(Qt::PreciseTimer);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(decodeThread, &VideoDecodeThread::frameReady,
            this, &VideoPlayerWidget::onFrame, Qt::DirectConnection);
    connect(decodeThread, &VideoDecodeThread::ended,
            this, &VideoPlayerWidget::onEnded, Qt::QueuedConnection);
    connect(decodeThread, &VideoDecodeThread::positionChanged,
            this, &VideoPlayerWidget::onPositionChanged, Qt::QueuedConnection);
    connect(decodeThread, &VideoDecodeThread::durationChanged,
            this, &VideoPlayerWidget::onDurationChanged, Qt::QueuedConnection);
    connect(&refreshTimer, &QTimer::timeout,
            this, &VideoPlayerWidget::onRefresh);
}

VideoPlayerWidget::~VideoPlayerWidget()
{
    decodeThread->stop();
    decodeThread->wait();
}

void VideoPlayerWidget::open(const QString &path)
{
    decodeThread->stop();
    decodeThread->wait();
    {
        QMutexLocker locker(&frameMutex);
        currentFrame = QImage();
        frames = 0;
    }
    statusText.clear();
    paused = false;
    fpsFrames = 0;
    displayFps = 0;
    positionMs = 0;
    durationMs = 0;
    fpsTimer.restart();
    const QString lower = path.toLower();
    liveSource = lower.contains(QStringLiteral(".m3u8"))
            || lower.startsWith(QStringLiteral("rtsp://"))
            || lower.startsWith(QStringLiteral("rtmp://"))
            || lower.startsWith(QStringLiteral("rtmps://"));
    decodeThread->setPaused(false);
    decodeThread->open(path);
    refreshTimer.start();
    update();
}

void VideoPlayerWidget::stop()
{
    decodeThread->stop();
    decodeThread->wait();
    refreshTimer.stop();
    {
        QMutexLocker locker(&frameMutex);
        currentFrame = QImage();
        frames = 0;
    }
    statusText.clear();
    paused = false;
    fpsFrames = 0;
    displayFps = 0;
    fpsTimer.restart();
    update();
}

void VideoPlayerWidget::togglePause()
{
    paused = !paused;
    decodeThread->setPaused(paused);
}

bool VideoPlayerWidget::isPaused() const
{
    return paused;
}

int VideoPlayerWidget::frameCount() const
{
    return frames;
}

QString VideoPlayerWidget::lastStatus() const
{
    return statusText;
}

bool VideoPlayerWidget::isLive() const
{
    return liveSource;
}

void VideoPlayerWidget::setRecordingStart(const QDateTime &start)
{
    recordingStart = start;
    update();
}

void VideoPlayerWidget::seek(qint64 ms)
{
    decodeThread->seek(ms);
}

qint64 VideoPlayerWidget::position() const
{
    return positionMs;
}

void VideoPlayerWidget::onPositionChanged(qint64 ms)
{
    positionMs = ms;
    emit positionChanged(ms);
    update();
}

void VideoPlayerWidget::onDurationChanged(qint64 ms)
{
    durationMs = ms;
    emit durationChanged(ms);
}

void VideoPlayerWidget::onFrame(const QImage &frame)
{
    {
        QMutexLocker locker(&frameMutex);
        currentFrame = frame;
        ++frames;
    }
    ++fpsFrames;
    if (fpsTimer.elapsed() >= 1000) {
        displayFps = qRound(fpsFrames * 1000.0 / fpsTimer.restart());
        fpsFrames = 0;
        qInfo().noquote() << QStringLiteral("[VideoPlayer] FPS=%1")
                .arg(displayFps);
        emit fpsChanged(displayFps);
    }
}

void VideoPlayerWidget::onRefresh()
{
    update();
}

void VideoPlayerWidget::onEnded(const QString &error)
{
    if (error == QStringLiteral("__stopped__"))
        return;
    if (!error.isEmpty())
        statusText = error;
    else
        statusText = tr("播放结束");
    update();
}

void VideoPlayerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0));

    QImage frame;
    {
        QMutexLocker locker(&frameMutex);
        frame = currentFrame;
    }

    if (!frame.isNull()) {
        const qreal scale = qMin(width() / static_cast<qreal>(frame.width()),
                                 height() / static_cast<qreal>(frame.height()));
        const int drawWidth = qRound(frame.width() * scale);
        const int drawHeight = qRound(frame.height() * scale);
        const QRectF target((width() - drawWidth) / 2.0,
                            (height() - drawHeight) / 2.0,
                            drawWidth, drawHeight);
        painter.drawImage(target, frame, QRectF(0, 0, frame.width(), frame.height()));

        if (!liveSource && recordingStart.isValid()) {
            const QDateTime stamp = recordingStart.addMSecs(positionMs);
            painter.setPen(QColor(255, 255, 255, 230));
            QFont font = painter.font();
            font.setPixelSize(16);
            painter.setFont(font);
            painter.drawText(8, height() - 12, stamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        }

        painter.setPen(QColor(46, 204, 113));
        QFont font = painter.font();
        font.setPixelSize(20);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(8, 24, tr("FPS %1").arg(displayFps));
        return;
    }

    if (!statusText.isEmpty()) {
        painter.setPen(QColor(150, 150, 155));
        painter.drawText(rect(), Qt::AlignCenter, statusText);
    }
}