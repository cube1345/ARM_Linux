#include "font_renderer.h"

#include "browser_log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 在两个颜色之间执行 8 位 alpha 混合。
 *
 * @param foreground 前景分量。
 * @param background 背景分量。
 * @param alpha 前景透明度。
 * @return 混合结果。
 */
static uint8_t blend_component(uint8_t foreground, uint8_t background,
                               uint8_t alpha)
{
    return (uint8_t)(((uint32_t)foreground * alpha +
                      (uint32_t)background * (255U - alpha) + 127U) / 255U);
}

/**
 * @brief 初始化 FreeType 并加载字体文件。
 *
 * @param renderer 字体上下文。
 * @param font_path TrueType/OpenType 字体路径。
 * @param pixel_size 字体像素高度。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_open(struct font_renderer *renderer,
                       const char *font_path, uint32_t pixel_size)
{
    if (renderer == NULL || font_path == NULL || pixel_size == 0) {
        errno = EINVAL;
        return -1;
    }
    memset(renderer, 0, sizeof(*renderer));
    if (FT_Init_FreeType(&renderer->library) != 0) {
        browser_log(BROWSER_LOG_ERROR, "FT_Init_FreeType failed");
        return -1;
    }
    if (FT_New_Face(renderer->library, font_path, 0, &renderer->face) != 0) {
        browser_log(BROWSER_LOG_ERROR, "cannot load font: %s", font_path);
        font_renderer_close(renderer);
        return -1;
    }
    if (font_renderer_set_size(renderer, pixel_size) < 0) {
        font_renderer_close(renderer);
        return -1;
    }
    return 0;
}

/**
 * @brief 修改字体像素高度。
 *
 * @param renderer 字体上下文。
 * @param pixel_size 新字体像素高度。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_set_size(struct font_renderer *renderer,
                           uint32_t pixel_size)
{
    if (renderer == NULL || renderer->face == NULL || pixel_size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (FT_Set_Pixel_Sizes(renderer->face, 0, pixel_size) != 0) {
        browser_log(BROWSER_LOG_ERROR, "FT_Set_Pixel_Sizes failed");
        return -1;
    }
    renderer->pixel_size = pixel_size;
    return 0;
}

/**
 * @brief 解码一个 UTF-8 字符。
 *
 * @param text 输入字节。
 * @param length 可用字节数。
 * @param codepoint 输出 Unicode codepoint。
 * @return 成功返回消耗字节数，非法输入返回 1 并输出替换字符。
 */
size_t font_renderer_decode_utf8(const uint8_t *text, size_t length,
                                 uint32_t *codepoint)
{
    uint32_t value;
    size_t count;
    size_t index;

    if (text == NULL || length == 0 || codepoint == NULL) {
        return 0;
    }
    if (text[0] < 0x80) {
        *codepoint = text[0];
        return 1;
    }
    if ((text[0] & 0xe0) == 0xc0) {
        value = text[0] & 0x1f;
        count = 2;
    } else if ((text[0] & 0xf0) == 0xe0) {
        value = text[0] & 0x0f;
        count = 3;
    } else if ((text[0] & 0xf8) == 0xf0) {
        value = text[0] & 0x07;
        count = 4;
    } else {
        *codepoint = 0xfffd;
        return 1;
    }
    if (count > length) {
        *codepoint = 0xfffd;
        return 1;
    }
    for (index = 1; index < count; index++) {
        if ((text[index] & 0xc0) != 0x80) {
            *codepoint = 0xfffd;
            return 1;
        }
        value = (value << 6) | (text[index] & 0x3f);
    }
    if ((count == 2 && value < 0x80) ||
        (count == 3 && value < 0x800) ||
        (count == 4 && value < 0x10000) ||
        value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        *codepoint = 0xfffd;
        return 1;
    }
    *codepoint = value;
    return count;
}

/**
 * @brief 获取一个 Unicode 字符的水平前进量。
 *
 * @param renderer 字体上下文。
 * @param codepoint Unicode codepoint。
 * @param advance 输出水平前进量。
 * @return 成功返回 0，失败返回 -1。
 */
int font_renderer_measure_codepoint(struct font_renderer *renderer,
                                    uint32_t codepoint, int *advance)
{
    if (renderer == NULL || renderer->face == NULL || advance == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (FT_Load_Char(renderer->face, codepoint, FT_LOAD_DEFAULT) != 0 &&
        FT_Load_Char(renderer->face, 0xfffd, FT_LOAD_DEFAULT) != 0) {
        return -1;
    }
    *advance = (int)(renderer->face->glyph->advance.x >> 6);
    if (*advance <= 0) {
        *advance = (int)renderer->pixel_size / 2;
    }
    return 0;
}

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
                                 int *advance)
{
    FT_GlyphSlot glyph;
    unsigned int row;

    if (renderer == NULL || renderer->face == NULL || display == NULL ||
        advance == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (FT_Load_Char(renderer->face, codepoint, FT_LOAD_RENDER) != 0) {
        if (FT_Load_Char(renderer->face, 0xfffd, FT_LOAD_RENDER) != 0) {
            return -1;
        }
    }
    glyph = renderer->face->glyph;
    *advance = (int)(glyph->advance.x >> 6);
    if (*advance <= 0) {
        *advance = (int)renderer->pixel_size / 2;
    }
    for (row = 0; row < glyph->bitmap.rows; row++) {
        unsigned int column;

        for (column = 0; column < glyph->bitmap.width; column++) {
            uint8_t alpha = glyph->bitmap.buffer[
                (size_t)row * (size_t)glyph->bitmap.pitch + column];
            uint8_t red;
            uint8_t green;
            uint8_t blue;

            if (alpha == 0) {
                continue;
            }
            red = blend_component((uint8_t)(foreground >> 16),
                                  (uint8_t)(background >> 16), alpha);
            green = blend_component((uint8_t)(foreground >> 8),
                                    (uint8_t)(background >> 8), alpha);
            blue = blend_component((uint8_t)foreground,
                                   (uint8_t)background, alpha);
            bmp_display_put_rgb(display, x + glyph->bitmap_left + (int)column,
                                baseline_y - glyph->bitmap_top + (int)row,
                                red, green, blue);
        }
    }
    return 0;
}

/**
 * @brief 释放字体与 FreeType 资源。
 *
 * @param renderer 字体上下文。
 */
void font_renderer_close(struct font_renderer *renderer)
{
    if (renderer == NULL) {
        return;
    }
    if (renderer->face != NULL) {
        FT_Done_Face(renderer->face);
    }
    if (renderer->library != NULL) {
        FT_Done_FreeType(renderer->library);
    }
    memset(renderer, 0, sizeof(*renderer));
}
