#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <libavcodec/avcodec.h>
#include <stddef.h>

/** @brief 视频 decoder 选择策略。 */
enum video_decoder_preference {
    VIDEO_DECODER_AUTO = 0,
    VIDEO_DECODER_SOFTWARE,
    VIDEO_DECODER_RKMPP
};

/** @brief 一次视频 decoder 选择结果。 */
struct video_decoder_selection {
    const char *backend_name;
    const char *codec_name;
    int hardware;
};

struct video_decoder_operation;

/** @brief 视频 decoder operation manager。 */
struct video_decoder_manager {
    struct video_decoder_operation *head;
    struct video_decoder_operation *tail;
    size_t count;
};

/** @brief 初始化视频 decoder manager。 */
void video_decoder_manager_init(struct video_decoder_manager *manager);

/**
 * @brief 注册内置 RKMPP 和 software decoder operation。
 * @param manager decoder manager。
 * @return 成功返回 0，失败返回 -1。
 */
int video_decoder_manager_register_builtin(
    struct video_decoder_manager *manager);

/**
 * @brief 解析 BROWSER_VIDEO_DECODER 选择策略。
 * @param output 输出策略。
 * @return 成功返回 0，值非法返回 -1。
 */
int video_decoder_preference_from_env(
    enum video_decoder_preference *output);

/**
 * @brief 按策略打开视频 decoder，auto 模式会自动回落 software。
 * @param manager decoder manager。
 * @param parameters FFmpeg stream codec 参数。
 * @param preference decoder 策略。
 * @param output 输出已打开的 codec context。
 * @param selection 输出 backend 信息。
 * @return 成功返回 0，无可用 decoder 返回 -1。
 */
int video_decoder_manager_open(
    const struct video_decoder_manager *manager,
    const AVCodecParameters *parameters,
    enum video_decoder_preference preference,
    AVCodecContext **output,
    struct video_decoder_selection *selection);

#endif
