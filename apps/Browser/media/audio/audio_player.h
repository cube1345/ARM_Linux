#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

struct audio_player;

/** @brief 可注册的音频播放 backend operation。 */
struct audio_backend_operation {
    const char *name;
    int (*supports)(const char *path);
    int (*play)(struct audio_player *player);
    struct audio_backend_operation *next;
};

/** @brief 音频 backend operation 链表管理器。 */
struct audio_backend_manager {
    struct audio_backend_operation *head;
    size_t count;
};

/** @brief 音频播放器运行状态。 */
enum audio_player_state {
    AUDIO_PLAYER_STOPPED = 0,
    AUDIO_PLAYER_PLAYING,
    AUDIO_PLAYER_PAUSED,
    AUDIO_PLAYER_ERROR
};

/** @brief 可供 UI 原子读取的播放器状态快照。 */
struct audio_player_status {
    enum audio_player_state state;
    int volume;
    uint64_t position_ms;
    uint64_t duration_ms;
};

/** @brief WAV/MP3 后台播放器上下文。 */
struct audio_player {
    struct audio_backend_manager backends;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    char path[4096];
    char device[128];
    enum audio_player_state state;
    uint64_t position_ms;
    uint64_t duration_ms;
    int64_t seek_ms;
    int volume;
    int stop_requested;
    int thread_created;
};

/** @brief 初始化音频 backend manager。 */
void audio_backend_manager_init(struct audio_backend_manager *manager);

/**
 * @brief 注册音频 backend，新注册 backend 优先匹配。
 * @param manager backend manager。
 * @param operation backend operation，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_backend_register(struct audio_backend_manager *manager,
                           struct audio_backend_operation *operation);

/** @brief 注册内置 WAV 和 MP3 backend。 */
int audio_backend_register_builtin(struct audio_backend_manager *manager);

/**
 * @brief 等待暂停结束并检查停止请求，供外部 backend 使用。
 * @return 应停止返回 1，否则返回 0。
 */
int audio_player_backend_wait(struct audio_player *player);

/**
 * @brief 取出一次 seek 请求，供外部 backend 使用。
 * @return 目标毫秒值，无请求返回 -1。
 */
int64_t audio_player_backend_take_seek(struct audio_player *player);

/** @brief 更新 backend 对外发布的总时长和当前位置。 */
void audio_player_backend_set_timing(struct audio_player *player,
                                     uint64_t duration_ms,
                                     uint64_t position_ms);

/**
 * @brief 初始化播放器同步资源和 mpg123。
 * @param player 播放器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_player_init(struct audio_player *player);

/**
 * @brief 启动 WAV 或 MP3 文件后台播放。
 * @param player 播放器上下文。
 * @param path 音频文件路径。
 * @param device ALSA PCM 设备名。
 * @return 成功返回 0，失败返回 -1。
 */
int audio_player_start(struct audio_player *player, const char *path,
                       const char *device);

/**
 * @brief 在播放与暂停状态之间切换。
 * @param player 播放器上下文。
 */
void audio_player_toggle_pause(struct audio_player *player);

/**
 * @brief 设置软件音量。
 * @param player 播放器上下文。
 * @param volume 音量百分比，自动限制到 0 到 100。
 */
void audio_player_set_volume(struct audio_player *player, int volume);

/**
 * @brief 请求跳转到总时长的指定百分比。
 * @param player 播放器上下文。
 * @param percent 目标百分比，自动限制到 0 到 100。
 */
void audio_player_seek_percent(struct audio_player *player, int percent);

/**
 * @brief 请求跳转到指定毫秒位置。
 * @param player 播放器上下文。
 * @param position_ms 目标位置，自动限制为非负值。
 */
void audio_player_seek_ms(struct audio_player *player, int64_t position_ms);

/**
 * @brief 原子读取播放器状态。
 * @param player 播放器上下文。
 * @param status 输出状态快照。
 */
void audio_player_get_status(struct audio_player *player,
                             struct audio_player_status *status);

/**
 * @brief 请求停止并等待播放线程退出。
 * @param player 播放器上下文。
 */
void audio_player_stop(struct audio_player *player);

/**
 * @brief 获取播放状态名称。
 * @param state 播放状态。
 * @return 静态字符串。
 */
const char *audio_player_state_name(enum audio_player_state state);

/**
 * @brief 停止播放并销毁同步资源和 mpg123。
 * @param player 播放器上下文。
 */
void audio_player_destroy(struct audio_player *player);

#endif
