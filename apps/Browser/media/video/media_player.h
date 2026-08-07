#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include "image_data.h"

#include <pthread.h>
#include <stdint.h>

/** @brief FFmpeg 播放器运行状态。 */
enum media_player_state {
    MEDIA_PLAYER_STOPPED = 0,
    MEDIA_PLAYER_PLAYING,
    MEDIA_PLAYER_PAUSED,
    MEDIA_PLAYER_ENDED,
    MEDIA_PLAYER_ERROR
};

/** @brief 可供页面读取的 FFmpeg 播放器状态。 */
struct media_player_status {
    enum media_player_state state;
    int volume;
    uint64_t position_ms;
    uint64_t duration_ms;
    uint64_t frame_serial;
    uint32_t width;
    uint32_t height;
    double frame_rate;
    int has_video;
};

/** @brief FFmpeg 多媒体后台播放器上下文。 */
struct media_player {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    char path[4096];
    char device[128];
    struct image_data frame;
    enum media_player_state state;
    uint64_t position_ms;
    uint64_t duration_ms;
    uint64_t frame_serial;
    uint32_t media_width;
    uint32_t media_height;
    double frame_rate;
    int volume;
    int64_t seek_ms;
    int stop_requested;
    int thread_created;
    int initialized;
};

/** @brief 初始化播放器同步资源。 */
int media_player_init(struct media_player *player);

/** @brief 启动指定媒体文件的 FFmpeg 播放线程。 */
int media_player_start(struct media_player *player, const char *path,
                       const char *device);

/** @brief 在播放与暂停状态之间切换。 */
void media_player_toggle_pause(struct media_player *player);

/** @brief 设置软件音量。 */
void media_player_set_volume(struct media_player *player, int volume);

/** @brief 按总时长百分比请求跳转。 */
void media_player_seek_percent(struct media_player *player, int percent);

/**
 * @brief 请求跳转到指定毫秒位置。
 * @param player 播放器上下文。
 * @param position_ms 目标位置，自动限制到媒体时长。
 */
void media_player_seek_ms(struct media_player *player, int64_t position_ms);

/** @brief 读取播放器状态快照。 */
void media_player_get_status(struct media_player *player,
                             struct media_player_status *status);

/** @brief 将最新视频帧复制到页面缓冲区，返回 1 表示有视频帧。 */
int media_player_copy_frame(struct media_player *player,
                            struct image_data *output,
                            uint64_t *serial);

/** @brief 停止并等待播放器线程退出。 */
void media_player_stop(struct media_player *player);

/** @brief 获取播放器状态名称。 */
const char *media_player_state_name(enum media_player_state state);

/** @brief 停止播放器并释放同步资源。 */
void media_player_destroy(struct media_player *player);

#endif
