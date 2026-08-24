#include "video_decoder.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_DECODER_ENV "BROWSER_VIDEO_DECODER"

/** @brief 视频 decoder backend operation。 */
struct video_decoder_operation {
    const char *name;
    enum video_decoder_preference preference;
    int hardware;
    const AVCodec *(*find)(enum AVCodecID codec_id);
    struct video_decoder_operation *next;
};

/** @brief 查找 RKMPP H.264/H.265 decoder。 */
static const AVCodec *find_rkmpp_decoder(enum AVCodecID codec_id)
{
    if (codec_id == AV_CODEC_ID_H264) {
        return avcodec_find_decoder_by_name("h264_rkmpp");
    }
    if (codec_id == AV_CODEC_ID_HEVC) {
        return avcodec_find_decoder_by_name("hevc_rkmpp");
    }
    return NULL;
}

/** @brief 查找明确的软件 decoder。 */
static const AVCodec *find_software_decoder(enum AVCodecID codec_id)
{
    if (codec_id == AV_CODEC_ID_H264) {
        return avcodec_find_decoder_by_name("h264");
    }
    if (codec_id == AV_CODEC_ID_HEVC) {
        return avcodec_find_decoder_by_name("hevc");
    }
    return avcodec_find_decoder(codec_id);
}

static struct video_decoder_operation rkmpp_operation = {
    "rkmpp", VIDEO_DECODER_RKMPP, 1, find_rkmpp_decoder, NULL
};
static struct video_decoder_operation software_operation = {
    "software", VIDEO_DECODER_SOFTWARE, 0, find_software_decoder, NULL
};

/** @brief 注册一个视频 decoder operation。 */
static int video_decoder_manager_register(
    struct video_decoder_manager *manager,
    struct video_decoder_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->find == NULL) {
        errno = EINVAL;
        return -1;
    }
    operation->next = NULL;
    if (manager->tail == NULL) {
        manager->head = operation;
    } else {
        manager->tail->next = operation;
    }
    manager->tail = operation;
    manager->count++;
    return 0;
}

/** @brief 初始化视频 decoder manager。 */
void video_decoder_manager_init(struct video_decoder_manager *manager)
{
    if (manager == NULL) return;
    manager->head = NULL;
    manager->tail = NULL;
    manager->count = 0;
}

/** @brief 注册内置 RKMPP 和 software decoder operation。 */
int video_decoder_manager_register_builtin(
    struct video_decoder_manager *manager)
{
    return video_decoder_manager_register(manager, &rkmpp_operation) < 0 ||
           video_decoder_manager_register(manager, &software_operation) < 0 ?
           -1 : 0;
}

/** @brief 解析 BROWSER_VIDEO_DECODER 选择策略。 */
int video_decoder_preference_from_env(
    enum video_decoder_preference *output)
{
    const char *value;

    if (output == NULL) {
        errno = EINVAL;
        return -1;
    }
    value = getenv(VIDEO_DECODER_ENV);
    if (value == NULL || value[0] == '\0' || strcmp(value, "auto") == 0) {
        *output = VIDEO_DECODER_AUTO;
        return 0;
    }
    if (strcmp(value, "software") == 0) {
        *output = VIDEO_DECODER_SOFTWARE;
        return 0;
    }
    if (strcmp(value, "rkmpp") == 0) {
        *output = VIDEO_DECODER_RKMPP;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

/** @brief 尝试用一个 backend 创建并打开 codec context。 */
static int open_with_operation(
    const struct video_decoder_operation *operation,
    const AVCodecParameters *parameters,
    AVCodecContext **output,
    struct video_decoder_selection *selection)
{
    const AVCodec *codec = operation->find(parameters->codec_id);
    AVCodecContext *context;

    if (codec == NULL) return 0;
    context = avcodec_alloc_context3(codec);
    if (context == NULL) return -1;
    if (avcodec_parameters_to_context(context, parameters) < 0 ||
        avcodec_open2(context, codec, NULL) < 0) {
        avcodec_free_context(&context);
        return 0;
    }
    *output = context;
    selection->backend_name = operation->name;
    selection->codec_name = codec->name;
    selection->hardware = operation->hardware;
    return 1;
}

/** @brief 按策略打开视频 decoder。 */
int video_decoder_manager_open(
    const struct video_decoder_manager *manager,
    const AVCodecParameters *parameters,
    enum video_decoder_preference preference,
    AVCodecContext **output,
    struct video_decoder_selection *selection)
{
    const struct video_decoder_operation *operation;

    if (manager == NULL || parameters == NULL || output == NULL ||
        selection == NULL) {
        errno = EINVAL;
        return -1;
    }
    *output = NULL;
    memset(selection, 0, sizeof(*selection));
    for (operation = manager->head; operation != NULL;
         operation = operation->next) {
        int result;

        if (preference != VIDEO_DECODER_AUTO &&
            operation->preference != preference) {
            continue;
        }
        result = open_with_operation(operation, parameters, output,
                                     selection);
        if (result > 0) return 0;
        if (result < 0) return -1;
    }
    errno = ENOTSUP;
    return -1;
}
