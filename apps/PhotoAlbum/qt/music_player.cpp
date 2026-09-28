#include "music_player.h"

#include <QDir>
#include <QFileInfo>
#include <QPainter>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <alsa/asoundlib.h>
}

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

AudioDecodeThread::AudioDecodeThread(QObject *parent)
    : QThread(parent), m_running(0), m_paused(0)
{
}

AudioDecodeThread::~AudioDecodeThread()
{
    stop();
    wait();
}

void AudioDecodeThread::play(const QString &path)
{
    m_path = path;
    start();
}

void AudioDecodeThread::stop()
{
    m_running.storeRelease(0);
}

void AudioDecodeThread::setPaused(bool paused)
{
    m_paused.storeRelease(paused ? 1 : 0);
}

void AudioDecodeThread::run()
{
    m_running.storeRelease(1);
    m_paused.storeRelease(0);

    av_register_all();
    AVFormatContext *format = nullptr;
    if (avformat_open_input(&format, m_path.toUtf8().constData(), nullptr, nullptr) < 0) {
        emit playbackFinished();
        return;
    }
    if (avformat_find_stream_info(format, nullptr) < 0) {
        avformat_close_input(&format);
        emit playbackFinished();
        return;
    }
    const int streamIdx = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIdx < 0) {
        avformat_close_input(&format);
        emit playbackFinished();
        return;
    }
    AVStream *stream = format->streams[streamIdx];
    AVCodecContext *codecCtx = stream->codec;
    const AVCodec *codec = avcodec_find_decoder(codecCtx->codec_id);
    if (!codec || avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avformat_close_input(&format);
        emit playbackFinished();
        return;
    }

    snd_pcm_t *pcm = nullptr;
    const int channels = codecCtx->channels > 0 ? codecCtx->channels : 2;
    openAlsaOutput(&pcm, codecCtx->sample_rate, channels);

    AVPacket packet;
    av_init_packet(&packet);
    AVFrame *frame = av_frame_alloc();
    QByteArray audioBuf;

    while (m_running.loadAcquire()) {
        if (m_paused.loadAcquire()) {
            msleep(40);
            continue;
        }
        const int read = av_read_frame(format, &packet);
        if (read < 0)
            break;
        if (packet.stream_index == streamIdx) {
            int got = 0;
            avcodec_decode_audio4(codecCtx, frame, &got, &packet);
            if (got && pcm) {
                const int ch = channels;
                const int samples = frame->nb_samples;
                audioBuf.resize(samples * ch * 2);
                int16_t *dst = reinterpret_cast<int16_t *>(audioBuf.data());
                if (frame->format == AV_SAMPLE_FMT_S16) {
                    memcpy(dst, frame->data[0], samples * ch * 2);
                } else if (frame->format == AV_SAMPLE_FMT_S16P) {
                    for (int c = 0; c < ch; ++c) {
                        const int16_t *src = reinterpret_cast<const int16_t *>(frame->data[c]);
                        for (int i = 0; i < samples; ++i)
                            dst[i * ch + c] = src[i];
                    }
                } else if (frame->format == AV_SAMPLE_FMT_FLTP) {
                    for (int c = 0; c < ch; ++c) {
                        const float *src = reinterpret_cast<const float *>(frame->data[c]);
                        for (int i = 0; i < samples; ++i) {
                            const float v = qBound(-1.0f, src[i], 1.0f);
                            dst[i * ch + c] = static_cast<int16_t>(v * 32767.0f);
                        }
                    }
                } else {
                    continue;
                }
                snd_pcm_writei(pcm, dst, samples);
            }
        }
        av_packet_unref(&packet);
    }

    av_frame_free(&frame);
    if (pcm)
        snd_pcm_close(pcm);
    avcodec_close(codecCtx);
    avformat_close_input(&format);
    emit playbackFinished();
}

MusicPlayer::MusicPlayer(QWidget *parent)
    : QWidget(parent),
      m_current(0),
      m_playing(false),
      m_thread(new AudioDecodeThread(this))
{
    setMinimumSize(400, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_thread, &AudioDecodeThread::playbackFinished, this, [this]() {
        if (m_playing)
            next();
    });
    loadSongs();
}

MusicPlayer::~MusicPlayer()
{
    stopAll();
}

void MusicPlayer::loadSongs()
{
    m_songs.clear();
    QDir dir(QStringLiteral("/home/root/music"));
    const QStringList filters = QStringList() << QStringLiteral("*.mp3") << QStringLiteral("*.wav")
                                             << QStringLiteral("*.flac") << QStringLiteral("*.ogg");
    const QFileInfoList list = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    foreach (const QFileInfo &info, list)
        m_songs.append(info.absoluteFilePath());
    m_current = 0;
}

void MusicPlayer::start()
{
    if (m_songs.isEmpty())
        return;
    playCurrent();
}

void MusicPlayer::stopAll()
{
    m_thread->stop();
    m_thread->wait();
    m_playing = false;
    update();
}

void MusicPlayer::playCurrent()
{
    if (m_current < 0 || m_current >= m_songs.size())
        return;
    m_thread->stop();
    m_thread->wait();
    m_thread->play(m_songs.at(m_current));
    m_playing = true;
    emit songChanged(QFileInfo(m_songs.at(m_current)).fileName());
    update();
}

void MusicPlayer::togglePlay()
{
    if (m_songs.isEmpty())
        return;
    if (m_playing) {
        m_thread->setPaused(true);
        m_playing = false;
    } else if (m_thread->isRunning()) {
        m_thread->setPaused(false);
        m_playing = true;
    } else {
        playCurrent();
    }
    update();
}

void MusicPlayer::next()
{
    if (m_songs.isEmpty())
        return;
    m_current = (m_current + 1) % m_songs.size();
    playCurrent();
}

void MusicPlayer::prev()
{
    if (m_songs.isEmpty())
        return;
    m_current = (m_current - 1 + m_songs.size()) % m_songs.size();
    playCurrent();
}

void MusicPlayer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0b, 0x0f));

    // 封面
    const int cover = qMin(width(), height()) / 2;
    const QRect coverRect((width() - cover) / 2, 40, cover, cover);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x30, 0x40, 0x60));
    p.drawRoundedRect(coverRect, 12, 12);
    p.setBrush(QColor(0x60, 0x80, 0xc0));
    const int cx = coverRect.center().x();
    const int cy = coverRect.center().y();
    p.drawEllipse(QPoint(cx, cy), cover / 3, cover / 3);
    p.setBrush(QColor(0x20, 0x20, 0x20));
    p.drawRect(cx - cover / 12, cy - cover / 4, cover / 6, cover / 2);

    // 歌名
    p.setPen(QColor(0xff, 0xff, 0xff));
    QFont f = p.font();
    f.setPixelSize(24);
    f.setBold(true);
    p.setFont(f);
    QString name = QStringLiteral("无音乐");
    if (m_current < m_songs.size())
        name = QFileInfo(m_songs.at(m_current)).fileName();
    p.drawText(rect().adjusted(0, coverRect.bottom() + 20, 0, 0), Qt::AlignHCenter | Qt::AlignTop, name);
}
