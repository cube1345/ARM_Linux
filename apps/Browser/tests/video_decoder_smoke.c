#include "video_decoder.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 验证 decoder 策略环境变量解析。 */
static int test_preferences(void)
{
    enum video_decoder_preference preference;

    unsetenv("BROWSER_VIDEO_DECODER");
    if (video_decoder_preference_from_env(&preference) < 0 ||
        preference != VIDEO_DECODER_AUTO) return -1;
    if (setenv("BROWSER_VIDEO_DECODER", "software", 1) < 0 ||
        video_decoder_preference_from_env(&preference) < 0 ||
        preference != VIDEO_DECODER_SOFTWARE) return -1;
    if (setenv("BROWSER_VIDEO_DECODER", "rkmpp", 1) < 0 ||
        video_decoder_preference_from_env(&preference) < 0 ||
        preference != VIDEO_DECODER_RKMPP) return -1;
    if (setenv("BROWSER_VIDEO_DECODER", "invalid", 1) < 0 ||
        video_decoder_preference_from_env(&preference) == 0 ||
        errno != EINVAL) return -1;
    unsetenv("BROWSER_VIDEO_DECODER");
    return 0;
}

/** @brief 验证 software 可用且 RKMPP 缺失时安全返回。 */
static int test_decoder_selection(void)
{
    struct video_decoder_manager manager;
    struct video_decoder_selection selection;
    AVCodecParameters *parameters = avcodec_parameters_alloc();
    AVCodecContext *context = NULL;

    if (parameters == NULL) return -1;
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = AV_CODEC_ID_H264;
    parameters->width = 320;
    parameters->height = 240;
    video_decoder_manager_init(&manager);
    if (video_decoder_manager_register_builtin(&manager) < 0 ||
        manager.count != 2U ||
        video_decoder_manager_open(&manager, parameters,
                                   VIDEO_DECODER_SOFTWARE,
                                   &context, &selection) < 0 ||
        context == NULL || selection.hardware ||
        strcmp(selection.backend_name, "software") != 0) {
        avcodec_free_context(&context);
        avcodec_parameters_free(&parameters);
        return -1;
    }
    avcodec_free_context(&context);
    if (video_decoder_manager_open(&manager, parameters,
                                   VIDEO_DECODER_AUTO,
                                   &context, &selection) < 0 ||
        context == NULL || selection.backend_name == NULL ||
        (strcmp(selection.backend_name, "software") != 0 &&
         strcmp(selection.backend_name, "rkmpp") != 0) ||
        selection.hardware !=
            (strcmp(selection.backend_name, "rkmpp") == 0)) {
        avcodec_free_context(&context);
        avcodec_parameters_free(&parameters);
        return -1;
    }
    avcodec_free_context(&context);
    if (video_decoder_manager_open(&manager, parameters,
                                   VIDEO_DECODER_RKMPP,
                                   &context, &selection) == 0) {
        if (context == NULL || !selection.hardware ||
            strcmp(selection.backend_name, "rkmpp") != 0) {
            avcodec_free_context(&context);
            avcodec_parameters_free(&parameters);
            return -1;
        }
        avcodec_free_context(&context);
    } else if (errno != ENOTSUP) {
        avcodec_parameters_free(&parameters);
        return -1;
    }
    avcodec_parameters_free(&parameters);
    return 0;
}

/** @brief 视频 decoder manager smoke test 入口。 */
int main(void)
{
    if (test_preferences() < 0 || test_decoder_selection() < 0) {
        fprintf(stderr, "FAIL video decoder manager\n");
        return EXIT_FAILURE;
    }
    printf("PASS video decoder manager\n");
    return EXIT_SUCCESS;
}
