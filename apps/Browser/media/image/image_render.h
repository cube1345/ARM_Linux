#ifndef IMAGE_RENDER_H
#define IMAGE_RENDER_H

#include "bmp_display.h"
#include "image_data.h"

/**
 * @brief 等比例缩放并居中绘制 RGB888 图片。
 *
 * @param display 显示设备上下文。
 * @param image 已解码的 RGB888 图片。
 * @param rotation 顺时针旋转角度，必须是 0、90、180 或 270。
 * @return 成功返回 0，失败返回 -1。
 */
int image_render_draw(struct bmp_display *display,
                      const struct image_data *image, unsigned int rotation);

/**
 * @brief 等比例缩放 RGB888 图片到指定最大尺寸。
 * @param source 源图片。
 * @param maximum_width 最大宽度。
 * @param maximum_height 最大高度。
 * @param output 输出图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int image_render_scale_fit(const struct image_data *source,
                           uint32_t maximum_width,
                           uint32_t maximum_height,
                           struct image_data *output);

/**
 * @brief 将 RGB888 图片居中绘制到指定矩形，不执行 flush。
 * @param display 显示设备。
 * @param image 已缩放图片。
 * @param x 矩形左上角 X。
 * @param y 矩形左上角 Y。
 * @param width 矩形宽度。
 * @param height 矩形高度。
 * @param background 空白区域颜色。
 * @return 成功返回 0，失败返回 -1。
 */
int image_render_draw_region(struct bmp_display *display,
                             const struct image_data *image,
                             int x, int y, int width, int height,
                             uint32_t background);

#endif
