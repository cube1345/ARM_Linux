#include "text_reader.h"

#include "ui_draw.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_MARGIN 24
#define TEXT_HEADER_HEIGHT 46
#define TEXT_BACKGROUND 0x101418U
#define TEXT_FOREGROUND 0xe8edf2U
#define TEXT_MUTED 0x98a4aeU

/**
 * @brief 跳过 UTF-8 BOM。
 *
 * @param data 文本数据。
 * @param size 数据大小。
 * @return 首个正文字节偏移。
 */
static size_t skip_bom(const uint8_t *data, size_t size)
{
    return size >= 3 && data[0] == 0xef && data[1] == 0xbb &&
           data[2] == 0xbf ? 3 : 0;
}

/**
 * @brief 读取 UTF-8 文本文件。
 *
 * @param reader 文本阅读器。
 * @param path 文本文件路径。
 * @return 成功返回 0，失败返回 -1。
 */
int text_reader_open(struct text_reader *reader, const char *path)
{
    FILE *file;
    long length;

    if (reader == NULL || path == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(reader, 0, sizeof(*reader));
    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        perror("inspect text file");
        fclose(file);
        return -1;
    }
    if ((unsigned long)length > TEXT_READER_MAX_FILE_SIZE) {
        fprintf(stderr, "text file exceeds %u bytes\n",
                TEXT_READER_MAX_FILE_SIZE);
        fclose(file);
        errno = EFBIG;
        return -1;
    }
    reader->data = malloc((size_t)length + 1);
    if (reader->data == NULL) {
        fclose(file);
        return -1;
    }
    if (fread(reader->data, 1, (size_t)length, file) != (size_t)length) {
        perror("read text file");
        fclose(file);
        text_reader_close(reader);
        return -1;
    }
    if (fclose(file) != 0) {
        text_reader_close(reader);
        return -1;
    }
    reader->data[length] = '\0';
    reader->size = (size_t)length;
    reader->page_offsets[0] = skip_bom(reader->data, reader->size);
    reader->page_count = 1;
    return 0;
}

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
                       struct font_renderer *font)
{
    size_t offset;
    int x = TEXT_MARGIN;
    int baseline_y = TEXT_HEADER_HEIGHT + (int)font->pixel_size;
    int right = (int)display->variable_info.xres - TEXT_MARGIN;
    int bottom = (int)display->variable_info.yres - TEXT_MARGIN;
    int line_height = (int)font->pixel_size + 8;
    char status[80];

    if (reader == NULL || reader->data == NULL || display == NULL ||
        font == NULL || reader->current_page >= reader->page_count) {
        errno = EINVAL;
        return -1;
    }
    bmp_display_clear(display, (uint8_t)(TEXT_BACKGROUND >> 16),
                      (uint8_t)(TEXT_BACKGROUND >> 8),
                      (uint8_t)TEXT_BACKGROUND);
    snprintf(status, sizeof(status), "TEXT  page %zu",
             reader->current_page + 1);
    ui_draw_text(display, font, status, TEXT_MARGIN,
                 (int)font->pixel_size + 8,
                 right - TEXT_MARGIN, TEXT_MUTED, TEXT_BACKGROUND);
    offset = reader->page_offsets[reader->current_page];

    while (offset < reader->size && baseline_y <= bottom) {
        uint32_t codepoint;
        size_t consumed = font_renderer_decode_utf8(
            reader->data + offset, reader->size - offset, &codepoint);
        int advance;

        if (consumed == 0) {
            break;
        }
        if (codepoint == '\r') {
            offset += consumed;
            continue;
        }
        if (codepoint == '\n') {
            x = TEXT_MARGIN;
            baseline_y += line_height;
            offset += consumed;
            continue;
        }
        if (font_renderer_measure_codepoint(font, codepoint, &advance) < 0) {
            return -1;
        }
        if (x + advance > right) {
            x = TEXT_MARGIN;
            baseline_y += line_height;
            if (baseline_y > bottom) {
                break;
            }
        }
        if (font_renderer_draw_codepoint(font, display, codepoint,
                                         x, baseline_y,
                                         TEXT_FOREGROUND, TEXT_BACKGROUND,
                                         &advance) < 0) {
            return -1;
        }
        x += advance;
        offset += consumed;
    }
    if (reader->current_page + 1 == reader->page_count &&
        offset < reader->size && reader->page_count < TEXT_READER_MAX_PAGES) {
        reader->page_offsets[reader->page_count++] = offset;
    }
    return bmp_display_flush(display);
}

/**
 * @brief 切换到下一页。
 *
 * @param reader 文本阅读器。
 * @return 已切换返回 1，到达末页返回 0。
 */
int text_reader_next(struct text_reader *reader)
{
    if (reader != NULL && reader->current_page + 1 < reader->page_count) {
        reader->current_page++;
        return 1;
    }
    return 0;
}

/**
 * @brief 切换到上一页。
 *
 * @param reader 文本阅读器。
 * @return 已切换返回 1，到达首页返回 0。
 */
int text_reader_previous(struct text_reader *reader)
{
    if (reader != NULL && reader->current_page > 0) {
        reader->current_page--;
        return 1;
    }
    return 0;
}

/**
 * @brief 释放文本数据。
 *
 * @param reader 文本阅读器。
 */
void text_reader_close(struct text_reader *reader)
{
    if (reader == NULL) {
        return;
    }
    free(reader->data);
    memset(reader, 0, sizeof(*reader));
}
