#include "video_player.h"

#include <QDebug>
#include <QPainter>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <alsa/asoundlib.h>
}

#if LIBAVCODEC_VERSION_MAJOR >= 58
#define USE_NEW_DECODE_API 1
#define USE_OLD_DECODE_API 0
#define AUDIO_CHANNELS(ctx) ((ctx)->ch_layout.nb_channels > 0 ? (ctx)->ch_layout.nb_channels : 2)
#else
#define USE_NEW_DECODE_API 0
#define USE_OLD_DECODE_API 1
#define AUDIO_CHANNELS(ctx) ((ctx)->channels)
#endif

static const char *kAlsaDevice = "plughw:0,0";

static bool openAlsaOutput(snd_pcm_t **pcm, int rate, int channels)
{
    snd_pcm_t *handle = nullptr;
    if (snd_pcm_open(&handle, kAlsaDevice, SND_PCM_STREAM_PLAYBACK, 0) < 0)
        return false;
    snd_pcm_hw_params_t *hw = nullptr;
    snd_pcm_hw_params_malloc(&hw);
    snd_pcm_hw_params_any(handle, hw);
    snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, hw, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, hw, channels);
    if (snd_pcm_hw_params_set_rate(handle, hw, rate, 0) < 0) {
        snd_pcm_hw_params_free(hw);
        snd_pcm_close(handle);
        return false;
    }
    if (snd_pcm_hw_params(handle, hw) < 0) {
        snd_pcm_hw_params_free(hw);
        snd_pcm_close(handle);
        return false;
    }
    snd_pcm_hw_params_free(hw);
    snd_pcm_prepare(handle);
    *pcm = handle;
    return true;
}

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
      lastPositionEmit(-1),
      audioCtx(nullptr),
      audioCtxOwned(false),
      audioStream(-1),
      alsaPcm(nullptr),
      audioEnabled(1)
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

void VideoDecodeThread::setAudioEnabled(bool on)
{
    audioEnabled.storeRelease(on ? 1 : 0);
}

bool VideoDecodeThread::audioOn() const
{
    return audioEnabled.loadAcquire() != 0;
}

void VideoDecodeThread::writeAudioFrames(AVFrame *aframe)
{
    if (!alsaPcm || !audioEnabled.loadAcquire() || !audioCtx)
        return;
    const int channels = AUDIO_CHANNELS(audioCtx) > 0 ? AUDIO_CHANNELS(audioCtx) : 2;
    const int samples = aframe->nb_samples;
    if (samples <= 0)
        return;
    audioBuffer.resize(samples * channels * 2);
    int16_t *dst = reinterpret_cast<int16_t *>(audioBuffer.data());
    if (aframe->format == AV_SAMPLE_FMT_S16) {
        memcpy(dst, aframe->data[0], static_cast<size_t>(samples) * channels * 2);
    } else if (aframe->format == AV_SAMPLE_FMT_S16P) {
        for (int c = 0; c < channels; ++c) {
            const int16_t *src = reinterpret_cast<const int16_t *>(aframe->data[c]);
            for (int i = 0; i < samples; ++i)
                dst[i * channels + c] = src[i];
        }
    } else if (aframe->format == AV_SAMPLE_FMT_FLTP) {
        for (int c = 0; c < channels; ++c) {
            const float *src = reinterpret_cast<const float *>(aframe->data[c]);
            for (int i = 0; i < samples; ++i) {
                const float v = qBound(-1.0f, src[i], 1.0f);
                dst[i * channels + c] = static_cast<int16_t>(v * 32767.0f);
            }
        }
    } else {
        return;
    }
    const int err = snd_pcm_writei(alsaPcm, dst, samples);
    if (err < 0) {
        if (err == -EPIPE || err == -ESTRPIPE) {
            snd_pcm_prepare(alsaPcm);
            snd_pcm_writei(alsaPcm, dst, samples);
        } else {
            static int audioErrCount = 0;
            if (++audioErrCount <= 3)
                qInfo().noquote() << QStringLiteral("[VideoPlayer] ALSA 写入错误: %1")
                        .arg(QString::fromUtf8(snd_strerror(err)));
        }
    } else {
        static qint64 audioWriteCount = 0;
        if ((++audioWriteCount % 500) == 1)
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 音频已写入 %1 帧").arg(audioWriteCount);
    }
}

void VideoDecodeThread::closeAudio()
{
    if (alsaPcm) {
        snd_pcm_close(alsaPcm);
        alsaPcm = nullptr;
    }
    if (audioCtx) {
        if (audioCtxOwned)
            avcodec_free_context(&audioCtx);
        audioCtx = nullptr;
    }
    audioStream = -1;
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
    AVFrame *aframe = av_frame_alloc();
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
        closeAudio();

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

        closeAudio();
        audioStream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audioStream >= 0) {
            AVStream *ast = format->streams[audioStream];
            const AVCodec *adec = avcodec_find_decoder(ast->codecpar->codec_id);
            if (adec) {
                audioCtx = avcodec_alloc_context3(adec);
                audioCtxOwned = true;
                if (avcodec_parameters_to_context(audioCtx, ast->codecpar) < 0
                        || avcodec_open2(audioCtx, nullptr, nullptr) < 0) {
                    avcodec_free_context(&audioCtx);
                    audioCtx = nullptr;
                }
                if (audioCtx)
                    openAlsaOutput(&alsaPcm, audioCtx->sample_rate, AUDIO_CHANNELS(audioCtx));
            }
        }
        if (audioCtx && alsaPcm)
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 音频输出已开启: %1Hz %2ch")
                    .arg(audioCtx->sample_rate).arg(AUDIO_CHANNELS(audioCtx));
        else if (audioCtx)
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 有音频流但 ALSA 打开失败");
        else
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 无音频流");

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
                    if (!audioCtx)
                        paceFrame(frameTimer, fps);
                    av_frame_unref(frame);
                }
            } else if (audioCtx && packet->stream_index == audioStream) {
                avcodec_send_packet(audioCtx, packet);
                av_packet_unref(packet);
                while (avcodec_receive_frame(audioCtx, aframe) == 0) {
                    writeAudioFrames(aframe);
                    av_frame_unref(aframe);
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
    closeAudio();
    if (format)
        avformat_close_input(&format);
    if (packet)
        av_packet_free(&packet);
    if (frame)
        av_frame_free(&frame);
    if (aframe)
        av_frame_free(&aframe);
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
    AVFrame *aframe = av_frame_alloc();
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
        closeAudio();

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

        closeAudio();
        audioStream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audioStream >= 0) {
            AVStream *ast = format->streams[audioStream];
            if (ast->codec) {
                audioCtx = ast->codec;
                audioCtxOwned = false;
                audioCtx->thread_count = 1;
                const AVCodec *adec = avcodec_find_decoder(audioCtx->codec_id);
                if (adec && avcodec_open2(audioCtx, adec, nullptr) >= 0) {
                    openAlsaOutput(&alsaPcm, audioCtx->sample_rate, AUDIO_CHANNELS(audioCtx));
                } else {
                    audioCtx = nullptr;
                }
            }
        }
        if (audioCtx && alsaPcm)
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 音频输出已开启: %1Hz %2ch")
                    .arg(audioCtx->sample_rate).arg(AUDIO_CHANNELS(audioCtx));
        else if (audioCtx)
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 有音频流但 ALSA 打开失败");
        else
            qInfo().noquote() << QStringLiteral("[VideoPlayer] 无音频流");

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
                    if (!audioCtx)
                        paceFrame(frameTimer, fps);
                }
            } else if (audioCtx && packet.stream_index == audioStream) {
                int gotAudio = 0;
                avcodec_decode_audio4(audioCtx, aframe, &gotAudio, &packet);
                av_packet_unref(&packet);
                if (gotAudio) {
                    writeAudioFrames(aframe);
                } else {
                    static int noAudioCount = 0;
                    if (++noAudioCount <= 5)
                        qInfo().noquote() << QStringLiteral("[VideoPlayer] 音频包解码无输出帧");
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
    closeAudio();
    if (format)
        avformat_close_input(&format);
    av_frame_free(&frame);
    av_frame_free(&aframe);
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

void VideoPlayerWidget::setAudioEnabled(bool on)
{
    decodeThread->setAudioEnabled(on);
}

bool VideoPlayerWidget::audioEnabled() const
{
    return decodeThread->audioOn();
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