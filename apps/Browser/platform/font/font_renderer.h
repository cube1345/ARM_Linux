#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H

#include <stddef.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "bmp_display.h"

/** @brief FreeType 字体渲染上下文。 */
struct font_renderer {
    FT_Library library;
    FT_Face face;
    uint32_t pixel_size;
};

/**
 * @brief 初始化 FreeType 并加载字体文件。
 *
 * @param renderer 字体上下文。
 * @param font_path TrueType/OpenType 字体路径。
 * @param pixel_size 字体像素高度。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_open(struct font_renderer *renderer,
                       const char *font_path, uint32_t pixel_size);

/**
 * @brief 修改字体像素高度。
 *
 * @param renderer 字体上下文。
 * @param pixel_size 新字体像素高度。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_set_size(struct font_renderer *renderer,
                           uint32_t pixel_size);

/**
 * @brief 解码一个 UTF-8 字符。
 *
 * @param text 输入字节。
 * @param length 可用字节数。
 * @param codepoint 输出 Unicode codepoint。
 * @return 成功返回消耗字节数，非法输入返回 1 并输出替换字符。
 */
size_t font_renderer_decode_utf8(const uint8_t *text, size_t length,
                                 uint32_t *codepoint);

/**
 * @brief 获取一个 Unicode 字符的水平前进量。
 *
 * @param renderer 字体上下文。
 * @param codepoint Unicode codepoint。
 * @param advance 输出水平前进量。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_measure_codepoint(struct font_renderer *renderer,
                                    uint32_t codepoint, int *advance);

/**
 * @brief 绘制单个 Unicode 字符。
 *
 * @param renderer 字体上下文。
 * @param display 显示设备。
 * @param codepoint Unicode codepoint。
 * @param x 字符基线起点 X。
 * @param baseline_y 字符基线 Y。
 * @param foreground 前景 RGB。
 * @param background 背景 RGB，用于抗锯齿混合。
 * @param advance 输出水平前进量。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_draw_codepoint(struct font_renderer *renderer,
                                 struct bmp_display *display,
                                 uint32_t codepoint, int x, int baseline_y,
                                 uint32_t foreground, uint32_t background,
                                 int *advance);

/**
 * @brief 释放字体与 FreeType 资源。
 *
 * @param renderer 字体上下文。
 */
void font_renderer_close(struct font_renderer *renderer);

#endif
