#include "audio_player.h"

#include "browser_log.h"

#include <alsa/asoundlib.h>
#include <errno.h>
#include <limits.h>
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define AUDIO_BUFFER_BYTES 16384U

/** @brief PCM WAV 文件参数。 */
struct wav_info {
    unsigned int channels;
    unsigned int rate;
    unsigned int bits;
    long data_offset;
    uint32_t data_size;
};

/** @brief 音频播放后端接口。 */
struct audio_backend {
    const char *name;
    int (*supports)(const char *path);
    int (*play)(struct audio_player *player);
};

/**
 * @brief 判断文件路径是否具有指定扩展名。
 * @param path 文件路径。
 * @param extension 扩展名，包含前导点。
 * @return 匹配返回 1，否则返回 0。
 */
static int has_extension(const char *path, const char *extension)
{
    const char *actual = strrchr(path, '.');

    return actual != NULL && strcasecmp(actual, extension) == 0;
}

/**
 * @brief 判断 WAV 后端是否支持指定路径。
 * @param path 音频文件路径。
 * @return 支持返回 1，否则返回 0。
 */
static int supports_wav(const char *path)
{
    return has_extension(path, ".wav");
}

/**
 * @brief 判断 MP3 后端是否支持指定路径。
 * @param path 音频文件路径。
 * @return 支持返回 1，否则返回 0。
 */
static int supports_mp3(const char *path)
{
    return has_extension(path, ".mp3");
}

/**
 * @brief 读取小端 16 位整数。
 * @param data 输入字节。
 * @return 解析值。
 */
static uint16_t le16(const unsigned char *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/**
 * @brief 读取小端 32 位整数。
 * @param data 输入字节。
 * @return 解析值。
 */
static uint32_t le32(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/**
 * @brief 等待暂停结束并检查停止请求。
 * @param player 播放器上下文。
 * @return 应停止返回 1，否则返回 0。
 */
static int wait_ready(struct audio_player *player)
{
    int stop;

    pthread_mutex_lock(&player->mutex);
    while (player->state == AUDIO_PLAYER_PAUSED && !player->stop_requested &&
           player->seek_ms < 0) {
        pthread_cond_wait(&player->condition, &player->mutex);
    }
    stop = player->stop_requested;
    pthread_mutex_unlock(&player->mutex);
    return stop;
}

/**
 * @brief 取出一次待处理 seek 请求。
 * @param player 播放器上下文。
 * @return 目标毫秒值，无请求返回 -1。
 */
static int64_t take_seek(struct audio_player *player)
{
    int64_t seek;

    pthread_mutex_lock(&player->mutex);
    seek = player->seek_ms;
    player->seek_ms = -1;
    pthread_mutex_unlock(&player->mutex);
    return seek;
}

/**
 * @brief 设置播放总时长和当前位置。
 * @param player 播放器上下文。
 * @param duration_ms 总时长毫秒。
 * @param position_ms 当前位置毫秒。
 */
static void set_timing(struct audio_player *player, uint64_t duration_ms,
                       uint64_t position_ms)
{
    pthread_mutex_lock(&player->mutex);
    player->duration_ms = duration_ms;
    player->position_ms = position_ms;
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 打开并配置 S16_LE ALSA 播放设备。
 * @param player 播放器上下文。
 * @param channels 声道数。
 * @param rate 采样率。
 * @param pcm 输出 PCM 句柄。
 * @return 成功返回 0，失败返回 -1。
 */
static int open_pcm(struct audio_player *player, unsigned int channels,
                    unsigned int rate, snd_pcm_t **pcm)
{
    int error = snd_pcm_open(pcm, player->device,
                             SND_PCM_STREAM_PLAYBACK, 0);

    if (error >= 0) {
        error = snd_pcm_set_params(*pcm, SND_PCM_FORMAT_S16_LE,
                                   SND_PCM_ACCESS_RW_INTERLEAVED,
                                   channels, rate, 1, 200000);
    }
    if (error < 0) {
        browser_log(BROWSER_LOG_ERROR, "ALSA setup: %s",
                    snd_strerror(error));
        if (*pcm != NULL) {
            snd_pcm_close(*pcm);
            *pcm = NULL;
        }
        return -1;
    }
    return 0;
}

/**
 * @brief 对 S16 样本应用软件音量并写入 ALSA。
 * @param player 播放器上下文。
 * @param pcm ALSA PCM 句柄。
 * @param samples 可修改的交错样本。
 * @param frames 帧数。
 * @param channels 声道数。
 * @param rate 采样率。
 * @return 成功返回已写帧数，失败返回 -1。
 */
static snd_pcm_sframes_t write_frames(struct audio_player *player,
                                      snd_pcm_t *pcm, int16_t *samples,
                                      size_t frames, unsigned int channels,
                                      unsigned int rate)
{
    size_t sample_count = frames * channels;
    size_t offset = 0;
    int volume;
    size_t index;

    pthread_mutex_lock(&player->mutex);
    volume = player->volume;
    pthread_mutex_unlock(&player->mutex);
    for (index = 0; index < sample_count; index++) {
        int scaled = (int)samples[index] * volume / 100;

        if (scaled > INT16_MAX) {
            scaled = INT16_MAX;
        } else if (scaled < INT16_MIN) {
            scaled = INT16_MIN;
        }
        samples[index] = (int16_t)scaled;
    }
    while (offset < frames) {
        snd_pcm_sframes_t written;
        int interrupted;

        if (wait_ready(player)) {
            break;
        }
        pthread_mutex_lock(&player->mutex);
        interrupted = player->seek_ms >= 0;
        pthread_mutex_unlock(&player->mutex);
        if (interrupted) {
            break;
        }
        written = snd_pcm_writei(pcm, samples + offset * channels,
                                 (snd_pcm_uframes_t)(frames - offset));
        if (written < 0) {
            written = snd_pcm_recover(pcm, (int)written, 1);
            if (written < 0) {
                browser_log(BROWSER_LOG_ERROR, "ALSA write: %s",
                            snd_strerror((int)written));
                return -1;
            }
            continue;
        }
        offset += (size_t)written;
        pthread_mutex_lock(&player->mutex);
        player->position_ms += (uint64_t)written * 1000U / rate;
        pthread_mutex_unlock(&player->mutex);
    }
    return (snd_pcm_sframes_t)offset;
}

/**
 * @brief 解析 PCM WAV 的 fmt 与 data chunk。
 * @param file 已打开文件。
 * @param info 输出 WAV 参数。
 * @return 成功返回 0，失败返回 -1。
 */
static int parse_wav(FILE *file, struct wav_info *info)
{
    unsigned char header[12];
    int have_format = 0;

    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        errno = EINVAL;
        return -1;
    }
    while (1) {
        unsigned char chunk[8];
        uint32_t size;

        if (fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk)) {
            break;
        }
        size = le32(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char format[16];

            if (size < sizeof(format) ||
                fread(format, 1, sizeof(format), file) != sizeof(format) ||
                le16(format) != 1) {
                return -1;
            }
            info->channels = le16(format + 2);
            info->rate = le32(format + 4);
            info->bits = le16(format + 14);
            have_format = 1;
            if (fseek(file, (long)(size - sizeof(format)) + (size & 1U),
                      SEEK_CUR) != 0) {
                return -1;
            }
        } else if (memcmp(chunk, "data", 4) == 0 && have_format) {
            info->data_offset = ftell(file);
            info->data_size = size;
            return info->data_offset >= 0 ? 0 : -1;
        } else if (fseek(file, (long)size + (size & 1U), SEEK_CUR) != 0) {
            return -1;
        }
    }
    errno = EINVAL;
    return -1;
}

/**
 * @brief 将一帧 PCM WAV 样本转换成 S16_LE。
 * @param source 原始样本首地址。
 * @param bits 原始位深。
 * @return S16 样本。
 */
static int16_t wav_sample(const unsigned char *source, unsigned int bits)
{
    if (bits == 8) {
        return (int16_t)(((int)source[0] - 128) << 8);
    }
    if (bits == 16) {
        return (int16_t)le16(source);
    }
    if (bits == 24) {
        int32_t value = (int32_t)((uint32_t)source[0] |
                        ((uint32_t)source[1] << 8) |
                        ((uint32_t)source[2] << 16));
        if ((value & 0x800000) != 0) {
            value |= (int32_t)0xff000000U;
        }
        return (int16_t)(value >> 8);
    }
    return (int16_t)((int32_t)le32(source) >> 16);
}

/**
 * @brief 播放 PCM WAV 文件。
 * @param player 播放器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int play_wav(struct audio_player *player)
{
    struct wav_info info = {0};
    snd_pcm_t *pcm = NULL;
    unsigned char *raw = NULL;
    int16_t *samples = NULL;
    FILE *file = NULL;
    uint64_t total_frames;
    uint64_t frame_position = 0;
    size_t source_sample_bytes;
    size_t frame_bytes;
    int result = -1;

    file = fopen(player->path, "rb");
    if (file == NULL || parse_wav(file, &info) < 0 || info.channels == 0 ||
        info.rate == 0 || (info.bits != 8 && info.bits != 16 &&
        info.bits != 24 && info.bits != 32)) {
        goto cleanup;
    }
    source_sample_bytes = info.bits / 8U;
    frame_bytes = source_sample_bytes * info.channels;
    total_frames = info.data_size / frame_bytes;
    set_timing(player, total_frames * 1000U / info.rate, 0);
    if (open_pcm(player, info.channels, info.rate, &pcm) < 0) {
        goto cleanup;
    }
    raw = malloc(AUDIO_BUFFER_BYTES);
    samples = malloc(AUDIO_BUFFER_BYTES * sizeof(*samples));
    if (raw == NULL || samples == NULL ||
        fseek(file, info.data_offset, SEEK_SET) != 0) {
        goto cleanup;
    }
    while (frame_position < total_frames && !wait_ready(player)) {
        int64_t seek_ms = take_seek(player);
        size_t wanted_frames;
        size_t bytes;
        size_t frames;
        size_t sample_count;
        size_t index;

        if (seek_ms >= 0) {
            frame_position = (uint64_t)seek_ms * info.rate / 1000U;
            if (frame_position > total_frames) {
                frame_position = total_frames;
            }
            if (fseek(file, info.data_offset +
                      (long)(frame_position * frame_bytes), SEEK_SET) != 0) {
                goto cleanup;
            }
            snd_pcm_drop(pcm);
            snd_pcm_prepare(pcm);
            set_timing(player, total_frames * 1000U / info.rate,
                       frame_position * 1000U / info.rate);
        }
        wanted_frames = AUDIO_BUFFER_BYTES / frame_bytes;
        if ((uint64_t)wanted_frames > total_frames - frame_position) {
            wanted_frames = (size_t)(total_frames - frame_position);
        }
        bytes = fread(raw, 1, wanted_frames * frame_bytes, file);
        frames = bytes / frame_bytes;
        if (frames == 0) {
            break;
        }
        sample_count = frames * info.channels;
        for (index = 0; index < sample_count; index++) {
            samples[index] = wav_sample(raw + index * source_sample_bytes,
                                        info.bits);
        }
        {
            snd_pcm_sframes_t written = write_frames(
                player, pcm, samples, frames, info.channels, info.rate);

            if (written < 0) {
                goto cleanup;
            }
            frame_position += (uint64_t)written;
            if ((size_t)written < frames) {
                if (fseek(file, info.data_offset +
                          (long)(frame_position * frame_bytes), SEEK_SET) != 0) {
                    goto cleanup;
                }
            }
        }
    }
    result = 0;

cleanup:
    free(samples);
    free(raw);
    if (pcm != NULL) {
        snd_pcm_drop(pcm);
        snd_pcm_close(pcm);
    }
    if (file != NULL) {
        fclose(file);
    }
    return result;
}

/**
 * @brief 播放 MP3 文件。
 * @param player 播放器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int play_mp3(struct audio_player *player)
{
    mpg123_handle *decoder = NULL;
    snd_pcm_t *pcm = NULL;
    unsigned char *buffer = NULL;
    long rate;
    int channels;
    int encoding;
    off_t length;
    size_t buffer_size;
    int decoder_error = MPG123_OK;
    int result = -1;

    decoder = mpg123_new(NULL, &decoder_error);
    if (decoder == NULL || mpg123_open(decoder, player->path) != MPG123_OK ||
        mpg123_getformat(decoder, &rate, &channels, &encoding) != MPG123_OK ||
        rate <= 0 || (channels != 1 && channels != 2)) {
        goto cleanup;
    }
    mpg123_format_none(decoder);
    if (mpg123_format(decoder, rate, channels, MPG123_ENC_SIGNED_16) !=
        MPG123_OK || open_pcm(player, (unsigned int)channels,
                             (unsigned int)rate, &pcm) < 0) {
        goto cleanup;
    }
    length = mpg123_length(decoder);
    set_timing(player, length > 0 ? (uint64_t)length * 1000U /
               (unsigned long)rate : 0, 0);
    buffer_size = mpg123_outblock(decoder);
    if (buffer_size < 4096U) {
        buffer_size = AUDIO_BUFFER_BYTES;
    }
    buffer = malloc(buffer_size);
    if (buffer == NULL) {
        goto cleanup;
    }
    while (!wait_ready(player)) {
        int64_t seek_ms = take_seek(player);
        size_t bytes = 0;
        int status;

        if (seek_ms >= 0) {
            off_t target = (off_t)((uint64_t)seek_ms *
                                   (unsigned long)rate / 1000U);
            off_t actual = mpg123_seek(decoder, target, SEEK_SET);

            if (actual < 0) {
                goto cleanup;
            }
            snd_pcm_drop(pcm);
            snd_pcm_prepare(pcm);
            set_timing(player, length > 0 ? (uint64_t)length * 1000U /
                       (unsigned long)rate : 0,
                       (uint64_t)actual * 1000U / (unsigned long)rate);
        }
        status = mpg123_read(decoder, buffer, buffer_size, &bytes);
        if (status == MPG123_DONE) {
            result = 0;
            break;
        }
        if (status == MPG123_NEW_FORMAT) {
            long new_rate;
            int new_channels;
            int new_encoding;

            if (mpg123_getformat(decoder, &new_rate, &new_channels,
                                 &new_encoding) != MPG123_OK ||
                new_rate != rate || new_channels != channels ||
                (new_encoding & MPG123_ENC_SIGNED_16) == 0) {
                browser_log(BROWSER_LOG_ERROR,
                            "unsupported MP3 format change");
                goto cleanup;
            }
            if (bytes == 0) {
                continue;
            }
        } else if (status != MPG123_OK) {
            browser_log(BROWSER_LOG_ERROR, "mpg123 decode: %s",
                        mpg123_strerror(decoder));
            goto cleanup;
        }
        if (bytes % ((size_t)channels * 2U) != 0) {
            errno = EINVAL;
            goto cleanup;
        }
        if (write_frames(player, pcm, (int16_t *)buffer,
                         bytes / ((size_t)channels * 2U),
                         (unsigned int)channels, (unsigned int)rate) < 0) {
            goto cleanup;
        }
    }
    if (wait_ready(player)) {
        result = 0;
    }

cleanup:
    free(buffer);
    if (pcm != NULL) {
        snd_pcm_drop(pcm);
        snd_pcm_close(pcm);
    }
    if (decoder != NULL) {
        mpg123_close(decoder);
        mpg123_delete(decoder);
    }
    return result;
}

/**
 * @brief 音频线程入口。
 * @param argument audio_player 指针。
 * @return 始终返回 NULL。
 */
static void *audio_thread(void *argument)
{
    struct audio_player *player = argument;
    const struct audio_backend backends[] = {
        {"wav", supports_wav, play_wav},
        {"mp3", supports_mp3, play_mp3}
    };
    int result = -1;
    size_t index;

    for (index = 0; index < sizeof(backends) / sizeof(backends[0]); index++) {
        if (backends[index].supports(player->path)) {
            result = backends[index].play(player);
            break;
        }
    }
    if (result < 0 && index == sizeof(backends) / sizeof(backends[0])) {
        errno = ENOTSUP;
    }

    pthread_mutex_lock(&player->mutex);
    if (!player->stop_requested) {
        player->state = result == 0 ? AUDIO_PLAYER_STOPPED : AUDIO_PLAYER_ERROR;
    }
    pthread_mutex_unlock(&player->mutex);
    return NULL;
}

/**
 * @brief 初始化播放器同步资源和 mpg123。
 * @param player 播放器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_player_init(struct audio_player *player)
{
    if (player == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(player, 0, sizeof(*player));
    player->seek_ms = -1;
    player->volume = 70;
    if (pthread_mutex_init(&player->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&player->condition, NULL) != 0) {
        pthread_mutex_destroy(&player->mutex);
        return -1;
    }
    if (mpg123_init() != MPG123_OK) {
        pthread_cond_destroy(&player->condition);
        pthread_mutex_destroy(&player->mutex);
        return -1;
    }
    return 0;
}

/**
 * @brief 启动 WAV 或 MP3 文件后台播放。
 * @param player 播放器上下文。
 * @param path 音频文件路径。
 * @param device ALSA PCM 设备名。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_player_start(struct audio_player *player, const char *path,
                       const char *device)
{
    if (player == NULL || path == NULL || device == NULL) {
        errno = EINVAL;
        return -1;
    }
    audio_player_stop(player);
    if (snprintf(player->path, sizeof(player->path), "%s", path) >=
            (int)sizeof(player->path) ||
        snprintf(player->device, sizeof(player->device), "%s", device) >=
            (int)sizeof(player->device)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    pthread_mutex_lock(&player->mutex);
    player->stop_requested = 0;
    player->seek_ms = -1;
    player->position_ms = 0;
    player->duration_ms = 0;
    player->state = AUDIO_PLAYER_PLAYING;
    pthread_mutex_unlock(&player->mutex);
    if (pthread_create(&player->thread, NULL, audio_thread, player) != 0) {
        pthread_mutex_lock(&player->mutex);
        player->state = AUDIO_PLAYER_ERROR;
        pthread_mutex_unlock(&player->mutex);
        return -1;
    }
    player->thread_created = 1;
    return 0;
}

/**
 * @brief 在播放与暂停状态之间切换。
 * @param player 播放器上下文。
 */
void audio_player_toggle_pause(struct audio_player *player)
{
    if (player == NULL) {
        return;
    }
    pthread_mutex_lock(&player->mutex);
    if (player->state == AUDIO_PLAYER_PLAYING) {
        player->state = AUDIO_PLAYER_PAUSED;
    } else if (player->state == AUDIO_PLAYER_PAUSED) {
        player->state = AUDIO_PLAYER_PLAYING;
        pthread_cond_broadcast(&player->condition);
    }
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 设置软件音量。
 * @param player 播放器上下文。
 * @param volume 音量百分比，自动限制到 0 到 100。
 */
void audio_player_set_volume(struct audio_player *player, int volume)
{
    if (player == NULL) {
        return;
    }
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    pthread_mutex_lock(&player->mutex);
    player->volume = volume;
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 请求跳转到总时长的指定百分比。
 * @param player 播放器上下文。
 * @param percent 目标百分比，自动限制到 0 到 100。
 */
void audio_player_seek_percent(struct audio_player *player, int percent)
{
    if (player == NULL) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    pthread_mutex_lock(&player->mutex);
    if (player->duration_ms > 0) {
        player->seek_ms = (int64_t)(player->duration_ms *
                                    (unsigned int)percent / 100U);
        player->position_ms = (uint64_t)player->seek_ms;
        pthread_cond_broadcast(&player->condition);
    }
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 原子读取播放器状态。
 * @param player 播放器上下文。
 * @param status 输出状态快照。
 */
void audio_player_get_status(struct audio_player *player,
                             struct audio_player_status *status)
{
    if (player == NULL || status == NULL) {
        return;
    }
    pthread_mutex_lock(&player->mutex);
    status->state = player->state;
    status->volume = player->volume;
    status->position_ms = player->position_ms;
    status->duration_ms = player->duration_ms;
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 请求停止并等待播放线程退出。
 * @param player 播放器上下文。
 */
void audio_player_stop(struct audio_player *player)
{
    int join;

    if (player == NULL) {
        return;
    }
    pthread_mutex_lock(&player->mutex);
    player->stop_requested = 1;
    pthread_cond_broadcast(&player->condition);
    join = player->thread_created;
    pthread_mutex_unlock(&player->mutex);
    if (join) {
        pthread_join(player->thread, NULL);
    }
    pthread_mutex_lock(&player->mutex);
    player->thread_created = 0;
    player->state = AUDIO_PLAYER_STOPPED;
    player->stop_requested = 0;
    player->seek_ms = -1;
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 获取播放状态名称。
 * @param state 播放状态。
 * @return 静态字符串。
 */
const char *audio_player_state_name(enum audio_player_state state)
{
    switch (state) {
    case AUDIO_PLAYER_PLAYING: return "PLAYING";
    case AUDIO_PLAYER_PAUSED: return "PAUSED";
    case AUDIO_PLAYER_ERROR: return "ERROR";
    case AUDIO_PLAYER_STOPPED:
    default: return "STOPPED";
    }
}

/**
 * @brief 停止播放并销毁同步资源和 mpg123。
 * @param player 播放器上下文。
 */
void audio_player_destroy(struct audio_player *player)
{
    if (player == NULL) {
        return;
    }
    audio_player_stop(player);
    pthread_cond_destroy(&player->condition);
    pthread_mutex_destroy(&player->mutex);
    mpg123_exit();
}
