#include "audio_player.h"

#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAV_BUFFER_SIZE 16384

/** @brief 已解析的 PCM WAV 参数。 */
struct wav_info {
    unsigned int channels;
    unsigned int rate;
    unsigned int bits;
    long data_offset;
    uint32_t data_size;
};

/**
 * @brief 从字节流读取小端 16 位整数。
 *
 * @param data 两字节数据。
 * @return 解码值。
 */
static uint16_t read_le16(const unsigned char *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/**
 * @brief 从字节流读取小端 32 位整数。
 *
 * @param data 四字节数据。
 * @return 解码值。
 */
static uint32_t read_le32(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/**
 * @brief 解析 PCM WAV 的 fmt 和 data chunk。
 *
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
        fprintf(stderr, "not a RIFF/WAVE file\n");
        return -1;
    }
    while (1) {
        unsigned char chunk[8];
        uint32_t size;

        if (fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk)) {
            break;
        }
        size = read_le32(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char format[16];

            if (size < sizeof(format) ||
                fread(format, 1, sizeof(format), file) != sizeof(format)) {
                return -1;
            }
            if (read_le16(format) != 1) {
                fprintf(stderr, "only PCM WAV is supported\n");
                return -1;
            }
            info->channels = read_le16(format + 2);
            info->rate = read_le32(format + 4);
            info->bits = read_le16(format + 14);
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
    fprintf(stderr, "WAV fmt/data chunk not found\n");
    return -1;
}

/**
 * @brief 将 WAV 位深转换成 ALSA 格式。
 *
 * @param bits PCM 位深。
 * @return ALSA 格式，不支持返回 SND_PCM_FORMAT_UNKNOWN。
 */
static snd_pcm_format_t wav_format(unsigned int bits)
{
    switch (bits) {
    case 8: return SND_PCM_FORMAT_U8;
    case 16: return SND_PCM_FORMAT_S16_LE;
    case 24: return SND_PCM_FORMAT_S24_3LE;
    case 32: return SND_PCM_FORMAT_S32_LE;
    default: return SND_PCM_FORMAT_UNKNOWN;
    }
}

/**
 * @brief 检查停止或等待暂停结束。
 *
 * @param player 播放器上下文。
 * @return 应停止返回 1，否则返回 0。
 */
static int wait_if_paused(struct audio_player *player)
{
    int stop;

    pthread_mutex_lock(&player->mutex);
    while (player->state == AUDIO_PLAYER_PAUSED && !player->stop_requested) {
        pthread_cond_wait(&player->condition, &player->mutex);
    }
    stop = player->stop_requested;
    pthread_mutex_unlock(&player->mutex);
    return stop;
}

/**
 * @brief 执行 WAV 文件 ALSA 播放。
 *
 * @param player 播放器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int play_wav(struct audio_player *player)
{
    struct wav_info info = {0};
    snd_pcm_t *pcm = NULL;
    snd_pcm_format_t format;
    unsigned char *buffer = NULL;
    FILE *file = NULL;
    uint32_t remaining;
    int result = -1;
    int error;

    file = fopen(player->path, "rb");
    if (file == NULL || parse_wav(file, &info) < 0) {
        goto cleanup;
    }
    format = wav_format(info.bits);
    if (format == SND_PCM_FORMAT_UNKNOWN || info.channels == 0 ||
        info.rate == 0) {
        fprintf(stderr, "unsupported WAV: %u ch, %u Hz, %u bit\n",
                info.channels, info.rate, info.bits);
        goto cleanup;
    }
    error = snd_pcm_open(&pcm, player->device, SND_PCM_STREAM_PLAYBACK, 0);
    if (error < 0) {
        fprintf(stderr, "snd_pcm_open: %s\n", snd_strerror(error));
        goto cleanup;
    }
    error = snd_pcm_set_params(pcm, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                               info.channels, info.rate, 1, 500000);
    if (error < 0) {
        fprintf(stderr, "snd_pcm_set_params: %s\n", snd_strerror(error));
        goto cleanup;
    }
    buffer = malloc(WAV_BUFFER_SIZE);
    if (buffer == NULL || fseek(file, info.data_offset, SEEK_SET) != 0) {
        goto cleanup;
    }
    remaining = info.data_size;
    while (remaining > 0 && !wait_if_paused(player)) {
        size_t bytes = remaining < WAV_BUFFER_SIZE ? remaining : WAV_BUFFER_SIZE;
        size_t frame_size = (size_t)info.channels * ((info.bits + 7U) / 8U);
        size_t frames;
        size_t offset = 0;

        bytes = fread(buffer, 1, bytes, file);
        if (bytes == 0) {
            break;
        }
        remaining -= (uint32_t)bytes;
        frames = bytes / frame_size;
        while (frames > 0 && !wait_if_paused(player)) {
            snd_pcm_sframes_t written = snd_pcm_writei(
                pcm, buffer + offset, (snd_pcm_uframes_t)frames);

            if (written < 0) {
                written = snd_pcm_recover(pcm, (int)written, 1);
                if (written < 0) {
                    fprintf(stderr, "ALSA write: %s\n", snd_strerror((int)written));
                    goto cleanup;
                }
                continue;
            }
            offset += (size_t)written * frame_size;
            frames -= (size_t)written;
        }
    }
    result = 0;

cleanup:
    free(buffer);
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
 * @brief 播放线程入口。
 *
 * @param argument audio_player 指针。
 * @return 始终返回 NULL。
 */
static void *audio_thread(void *argument)
{
    struct audio_player *player = argument;
    int result = play_wav(player);

    pthread_mutex_lock(&player->mutex);
    player->state = result == 0 ? AUDIO_PLAYER_STOPPED : AUDIO_PLAYER_ERROR;
    pthread_mutex_unlock(&player->mutex);
    return NULL;
}

/**
 * @brief 初始化播放器同步资源。
 *
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
    if (pthread_mutex_init(&player->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&player->condition, NULL) != 0) {
        pthread_mutex_destroy(&player->mutex);
        return -1;
    }
    return 0;
}

/**
 * @brief 启动一个 WAV 文件的后台播放。
 *
 * @param player 播放器上下文。
 * @param path WAV 文件路径。
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
    player->stop_requested = 0;
    player->state = AUDIO_PLAYER_PLAYING;
    if (pthread_create(&player->thread, NULL, audio_thread, player) != 0) {
        player->state = AUDIO_PLAYER_ERROR;
        return -1;
    }
    player->thread_created = 1;
    return 0;
}

/**
 * @brief 在播放与暂停状态之间切换。
 *
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
 * @brief 请求停止并等待播放线程退出。
 *
 * @param player 播放器上下文。
 */
void audio_player_stop(struct audio_player *player)
{
    if (player == NULL || !player->thread_created) {
        return;
    }
    pthread_mutex_lock(&player->mutex);
    player->stop_requested = 1;
    pthread_cond_broadcast(&player->condition);
    pthread_mutex_unlock(&player->mutex);
    pthread_join(player->thread, NULL);
    player->thread_created = 0;
    player->state = AUDIO_PLAYER_STOPPED;
}

/**
 * @brief 获取当前播放状态。
 *
 * @param player 播放器上下文。
 * @return 播放状态。
 */
enum audio_player_state audio_player_get_state(struct audio_player *player)
{
    enum audio_player_state state;

    if (player == NULL) {
        return AUDIO_PLAYER_ERROR;
    }
    pthread_mutex_lock(&player->mutex);
    state = player->state;
    pthread_mutex_unlock(&player->mutex);
    return state;
}

/**
 * @brief 获取播放状态名称。
 *
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
 * @brief 停止播放并销毁同步资源。
 *
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
    memset(player, 0, sizeof(*player));
}
