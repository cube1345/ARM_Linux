#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <pthread.h>
#include <stdint.h>

/** @brief WAV 播放器运行状态。 */
enum audio_player_state {
    AUDIO_PLAYER_STOPPED = 0,
    AUDIO_PLAYER_PLAYING,
    AUDIO_PLAYER_PAUSED,
    AUDIO_PLAYER_ERROR
};

/** @brief ALSA WAV 播放器上下文。 */
struct audio_player {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    char path[4096];
    char device[128];
    enum audio_player_state state;
    int stop_requested;
    int thread_created;
};

/**
 * @brief 初始化播放器同步资源。
 *
 * @param player 播放器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_player_init(struct audio_player *player);

/**
 * @brief 启动一个 WAV 文件的后台播放。
 *
 * @param player 播放器上下文。
 * @param path WAV 文件路径。
 * @param device ALSA PCM 设备名，例如 default 或 hw:0,0。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_player_start(struct audio_player *player, const char *path,
                       const char *device);

/**
 * @brief 在播放与暂停状态之间切换。
 *
 * @param player 播放器上下文。
 */
void audio_player_toggle_pause(struct audio_player *player);

/**
 * @brief 请求停止并等待播放线程退出。
 *
 * @param player 播放器上下文。
 */
void audio_player_stop(struct audio_player *player);

/**
 * @brief 获取当前播放状态。
 *
 * @param player 播放器上下文。
 * @return 播放状态。
 */
enum audio_player_state audio_player_get_state(struct audio_player *player);

/**
 * @brief 获取播放状态名称。
 *
 * @param state 播放状态。
 * @return 静态字符串。
 */
const char *audio_player_state_name(enum audio_player_state state);

/**
 * @brief 停止播放并销毁同步资源。
 *
 * @param player 播放器上下文。
 */
void audio_player_destroy(struct audio_player *player);

#endif
