#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H

#include "file_list.h"
#include "image_data.h"

/** @brief 单个静态图片解码器操作接口。 */
struct image_decoder {
    const char *name;
    int (*supports)(enum file_type type);
    int (*decode)(const char *path, struct image_data *image);
    struct image_decoder *next;
};

/** @brief 静态图片解码器注册管理器。 */
struct image_decoder_manager {
    struct image_decoder *head;
};

/**
 * @brief 初始化图片解码器管理器。
 *
 * @param manager 解码器管理器。
 */
void image_decoder_manager_init(struct image_decoder_manager *manager);

/**
 * @brief 注册一个静态图片解码器。
 *
 * @param manager 解码器管理器。
 * @param decoder 待注册解码器，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_register(struct image_decoder_manager *manager,
                           struct image_decoder *decoder);

/**
 * @brief 注册内置 BMP、JPEG 和 PNG 解码器。
 *
 * @param manager 解码器管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_register_builtin(struct image_decoder_manager *manager);

/**
 * @brief 提前初始化默认图片解码器管理器。
 *
 * @return 成功返回 0，失败返回 -1。
 */
int image_decoder_prepare(void);

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
                                 struct image_data *image);

/**
 * @brief 按文件类型调用对应图片解码器。
 *
 * @param path 图片路径。
 * @param type 图片文件类型。
 * @param image 输出 RGB888 图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decode(const char *path, enum file_type type,
                 struct image_data *image);

#endif
