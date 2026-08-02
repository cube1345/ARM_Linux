#ifndef TEXT_READER_H
#define TEXT_READER_H

#include <stddef.h>
#include <stdint.h>

#include "font_renderer.h"

#define TEXT_READER_MAX_FILE_SIZE (4U * 1024U * 1024U)
#define TEXT_READER_MAX_PAGES 4096

/** @brief 内存中的 UTF-8 文本及分页状态。 */
struct text_reader {
    uint8_t *data;
    size_t size;
    size_t page_offsets[TEXT_READER_MAX_PAGES];
    size_t page_count;
    size_t current_page;
};

/**
 * @brief 读取 UTF-8 文本文件。
 *
 * @param reader 文本阅读器。
 * @param path 文本文件路径。
 * @return 成功返回 0，失败返回 -1。
 */
int text_reader_open(struct text_reader *reader, const char *path);

/**
 * @brief 绘制当前文本页并记录下一页偏移。
 *
 * @param reader 文本阅读器。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int text_reader_render(struct text_reader *reader,
                       struct bmp_display *display,
                       struct font_renderer *font);

/**
 * @brief 切换到下一页。
 *
 * @param reader 文本阅读器。
 * @return 已切换返回 1，到达末页返回 0。
 */
int text_reader_next(struct text_reader *reader);

/**
 * @brief 切换到上一页。
 *
 * @param reader 文本阅读器。
 * @return 已切换返回 1，到达首页返回 0。
 */
int text_reader_previous(struct text_reader *reader);

/**
 * @brief 释放文本数据。
 *
 * @param reader 文本阅读器。
 */
void text_reader_close(struct text_reader *reader);

#endif
