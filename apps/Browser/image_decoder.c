#include "image_decoder.h"

#include "bmp_decoder.h"
#include "jpeg_decoder.h"
#include "png_decoder.h"

#include <errno.h>
#include <stddef.h>

/** @brief 默认全局图片解码器管理器。 */
static struct image_decoder_manager default_manager;

/** @brief 默认管理器是否已经完成内置注册。 */
static int default_manager_ready;

/** @brief 内置 BMP 解码器对象。 */
static struct image_decoder bmp_decoder;

/** @brief 内置 JPEG 解码器对象。 */
static struct image_decoder jpeg_decoder;

/** @brief 内置 PNG 解码器对象。 */
static struct image_decoder png_decoder;

/**
 * @brief 判断 BMP 解码器是否支持指定类型。
 *
 * @param type 文件类型。
 * @return 支持返回 1，否则返回 0。
 */
static int supports_bmp(enum file_type type)
{
    return type == FILE_TYPE_BMP;
}

/**
 * @brief 判断 JPEG 解码器是否支持指定类型。
 *
 * @param type 文件类型。
 * @return 支持返回 1，否则返回 0。
 */
static int supports_jpeg(enum file_type type)
{
    return type == FILE_TYPE_JPEG;
}

/**
 * @brief 判断 PNG 解码器是否支持指定类型。
 *
 * @param type 文件类型。
 * @return 支持返回 1，否则返回 0。
 */
static int supports_png(enum file_type type)
{
    return type == FILE_TYPE_PNG;
}

/**
 * @brief 初始化图片解码器管理器。
 *
 * @param manager 解码器管理器。
 */
void image_decoder_manager_init(struct image_decoder_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
    }
}

/**
 * @brief 注册一个静态图片解码器。
 *
 * @param manager 解码器管理器。
 * @param decoder 待注册解码器，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_register(struct image_decoder_manager *manager,
                           struct image_decoder *decoder)
{
    if (manager == NULL || decoder == NULL || decoder->name == NULL ||
        decoder->supports == NULL || decoder->decode == NULL) {
        errno = EINVAL;
        return -1;
    }
    decoder->next = manager->head;
    manager->head = decoder;
    return 0;
}

/**
 * @brief 注册内置 BMP、JPEG 和 PNG 解码器。
 *
 * @param manager 解码器管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_register_builtin(struct image_decoder_manager *manager)
{
    bmp_decoder.name = "bmp";
    bmp_decoder.supports = supports_bmp;
    bmp_decoder.decode = bmp_decode;
    bmp_decoder.next = NULL;
    jpeg_decoder.name = "jpeg";
    jpeg_decoder.supports = supports_jpeg;
    jpeg_decoder.decode = jpeg_decode;
    jpeg_decoder.next = NULL;
    png_decoder.name = "png";
    png_decoder.supports = supports_png;
    png_decoder.decode = png_decode;
    png_decoder.next = NULL;
    return image_decoder_register(manager, &bmp_decoder) < 0 ||
           image_decoder_register(manager, &jpeg_decoder) < 0 ||
           image_decoder_register(manager, &png_decoder) < 0 ? -1 : 0;
}

/**
 * @brief 通过指定管理器解码静态图片。
 *
 * @param manager 解码器管理器。
 * @param path 图片路径。
 * @param type 图片文件类型。
 * @param image 输出 RGB888 图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_manager_decode(struct image_decoder_manager *manager,
                                 const char *path, enum file_type type,
                                 struct image_data *image)
{
    struct image_decoder *decoder;

    if (manager == NULL || path == NULL || image == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (decoder = manager->head; decoder != NULL; decoder = decoder->next) {
        if (decoder->supports(type)) {
            return decoder->decode(path, image);
        }
    }
    errno = ENOTSUP;
    return -1;
}

/**
 * @brief 确保默认全局图片解码器管理器已注册内置模块。
 *
 * @return 成功返回 0，失败返回 -1。
 */
static int ensure_default_manager(void)
{
    if (!default_manager_ready) {
        image_decoder_manager_init(&default_manager);
        if (image_decoder_register_builtin(&default_manager) < 0) {
            return -1;
        }
        default_manager_ready = 1;
    }
    return 0;
}

/**
 * @brief 提前初始化默认图片解码器管理器。
 *
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_prepare(void)
{
    return ensure_default_manager();
}

/**
 * @brief 按文件类型调用对应图片解码器。
 *
 * @param path 图片路径。
 * @param type 图片文件类型。
 * @param image 输出 RGB888 图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decode(const char *path, enum file_type type,
                 struct image_data *image)
{
    if (ensure_default_manager() < 0) {
        return -1;
    }
    return image_decoder_manager_decode(&default_manager, path, type, image);
}
