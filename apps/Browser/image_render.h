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

#endif
