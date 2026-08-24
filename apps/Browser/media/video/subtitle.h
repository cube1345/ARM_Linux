#ifndef SUBTITLE_H
#define SUBTITLE_H

#include <stddef.h>
#include <stdint.h>

#define SUBTITLE_TEXT_SIZE 512U

/** @brief 一条带起止时间的 UTF-8 字幕。 */
struct subtitle_entry {
    uint64_t start_ms;
    uint64_t end_ms;
    char text[SUBTITLE_TEXT_SIZE];
};

/** @brief 当前媒体的 sidecar 字幕集合。 */
struct subtitle_track {
    struct subtitle_entry *entries;
    size_t count;
};

/**
 * @brief 自动加载与媒体同 basename 的 .srt 字幕。
 * @param track 字幕集合。
 * @param media_path 媒体文件路径。
 * @return 加载成功或字幕不存在返回 0，解析/读取失败返回 -1。
 */
int subtitle_track_load_for_media(struct subtitle_track *track,
                                  const char *media_path);

/**
 * @brief 查找指定播放位置应显示的字幕。
 * @param track 字幕集合。
 * @param position_ms 播放位置毫秒值。
 * @return 命中字幕文本，无字幕返回 NULL。
 */
const char *subtitle_track_text_at(const struct subtitle_track *track,
                                   uint64_t position_ms);

/**
 * @brief 释放字幕集合。
 * @param track 字幕集合。
 */
void subtitle_track_close(struct subtitle_track *track);

#endif
