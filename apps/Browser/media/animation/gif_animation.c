#include "gif_animation.h"

#include <errno.h>
#include <gif_lib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 从 GIF 扩展块读取 NETSCAPE 循环次数。
 * @param gif 已由 DGifSlurp 完整读取的 GIF。
 * @return 循环次数，0 表示无限，未声明时返回 1。
 */
static unsigned int read_loop_count(const GifFileType *gif)
{
    int image_index;

    for (image_index = 0; image_index < gif->ImageCount; image_index++) {
        const SavedImage *saved = &gif->SavedImages[image_index];
        int extension_index;
        int netscape = 0;

        for (extension_index = 0;
             extension_index < saved->ExtensionBlockCount;
             extension_index++) {
            const ExtensionBlock *block =
                &saved->ExtensionBlocks[extension_index];

            if (block->Function == APPLICATION_EXT_FUNC_CODE &&
                block->ByteCount >= 11 &&
                (memcmp(block->Bytes, "NETSCAPE2.0", 11) == 0 ||
                 memcmp(block->Bytes, "ANIMEXTS1.0", 11) == 0)) {
                netscape = 1;
            } else if (netscape && block->ByteCount >= 3 &&
                       block->Bytes[0] == 1) {
                unsigned int repeats = (unsigned int)block->Bytes[1] |
                    ((unsigned int)block->Bytes[2] << 8);

                return repeats == 0 ? 0 : repeats + 1U;
            }
        }
    }
    return 1;
}

/**
 * @brief 用指定 RGB 颜色填充画布矩形。
 * @param canvas RGB888 画布。
 * @param canvas_width 画布宽度。
 * @param canvas_height 画布高度。
 * @param left 矩形左边界。
 * @param top 矩形上边界。
 * @param width 矩形宽度。
 * @param height 矩形高度。
 * @param color 三字节 RGB 颜色。
 */
static void fill_rect(uint8_t *canvas, int canvas_width, int canvas_height,
                      int left, int top, int width, int height,
                      const uint8_t color[3])
{
    int y;

    for (y = 0; y < height; y++) {
        int target_y = top + y;
        int x;

        if (target_y < 0 || target_y >= canvas_height) {
            continue;
        }
        for (x = 0; x < width; x++) {
            int target_x = left + x;
            uint8_t *pixel;

            if (target_x < 0 || target_x >= canvas_width) {
                continue;
            }
            pixel = canvas + ((size_t)target_y * (size_t)canvas_width +
                              (size_t)target_x) * 3U;
            memcpy(pixel, color, 3);
        }
    }
}

/**
 * @brief 将一个 GIF 局部帧按透明索引绘制到画布。
 * @param gif GIF 对象。
 * @param saved 局部帧。
 * @param transparent 透明调色板索引，-1 表示无透明色。
 * @param canvas RGB888 画布。
 * @return 成功返回 0，格式错误返回 -1。
 */
static int draw_saved_image(const GifFileType *gif, const SavedImage *saved,
                            int transparent, uint8_t *canvas)
{
    const GifImageDesc *descriptor = &saved->ImageDesc;
    const ColorMapObject *map = descriptor->ColorMap != NULL ?
                                descriptor->ColorMap : gif->SColorMap;
    int y;

    if (map == NULL || saved->RasterBits == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (y = 0; y < descriptor->Height; y++) {
        int x;

        for (x = 0; x < descriptor->Width; x++) {
            int index = saved->RasterBits[(size_t)y * descriptor->Width + x];
            int target_x = descriptor->Left + x;
            int target_y = descriptor->Top + y;
            uint8_t *pixel;
            const GifColorType *color;

            if (index == transparent) {
                continue;
            }
            if (index < 0 || index >= map->ColorCount || target_x < 0 ||
                target_x >= gif->SWidth || target_y < 0 ||
                target_y >= gif->SHeight) {
                errno = EINVAL;
                return -1;
            }
            color = &map->Colors[index];
            pixel = canvas + ((size_t)target_y * gif->SWidth + target_x) * 3U;
            pixel[0] = color->Red;
            pixel[1] = color->Green;
            pixel[2] = color->Blue;
        }
    }
    return 0;
}

/**
 * @brief 解码 GIF 并预合成全部动画帧。
 * @param animation 输出动画，调用前必须清零。
 * @param path GIF 文件路径。
 * @return 成功返回 0，失败返回 -1。
 */
int gif_animation_open(struct gif_animation *animation, const char *path)
{
    GifFileType *gif = NULL;
    uint8_t *canvas = NULL;
    uint8_t *previous = NULL;
    uint8_t background[3] = {0, 0, 0};
    size_t canvas_size;
    int gif_error = 0;
    int index;
    int result = -1;

    if (animation == NULL || path == NULL || animation->frames != NULL) {
        errno = EINVAL;
        return -1;
    }
    gif = DGifOpenFileName(path, &gif_error);
    if (gif == NULL || DGifSlurp(gif) != GIF_OK || gif->ImageCount <= 0 ||
        gif->SWidth <= 0 || gif->SHeight <= 0 ||
        (size_t)gif->SWidth > SIZE_MAX / 3U ||
        (size_t)gif->SHeight > SIZE_MAX / ((size_t)gif->SWidth * 3U)) {
        errno = EINVAL;
        goto cleanup;
    }
    canvas_size = (size_t)gif->SWidth * (size_t)gif->SHeight * 3U;
    canvas = calloc(1, canvas_size);
    previous = malloc(canvas_size);
    animation->frames = calloc((size_t)gif->ImageCount,
                               sizeof(*animation->frames));
    if (canvas == NULL || previous == NULL || animation->frames == NULL) {
        goto cleanup;
    }
    animation->frame_count = (size_t)gif->ImageCount;
    animation->loop_count = read_loop_count(gif);
    if (gif->SColorMap != NULL && gif->SBackGroundColor >= 0 &&
        gif->SBackGroundColor < gif->SColorMap->ColorCount) {
        const GifColorType *color =
            &gif->SColorMap->Colors[gif->SBackGroundColor];

        background[0] = color->Red;
        background[1] = color->Green;
        background[2] = color->Blue;
        fill_rect(canvas, gif->SWidth, gif->SHeight, 0, 0,
                  gif->SWidth, gif->SHeight, background);
    }
    for (index = 0; index < gif->ImageCount; index++) {
        SavedImage *saved = &gif->SavedImages[index];
        GraphicsControlBlock control = {
            DISPOSAL_UNSPECIFIED, false, 10, NO_TRANSPARENT_COLOR
        };
        struct gif_frame *frame = &animation->frames[index];

        (void)DGifSavedExtensionToGCB(gif, index, &control);
        if (control.DisposalMode == DISPOSE_PREVIOUS) {
            memcpy(previous, canvas, canvas_size);
        }
        if (draw_saved_image(gif, saved, control.TransparentColor,
                             canvas) < 0 ||
            image_data_create(&frame->image, (uint32_t)gif->SWidth,
                              (uint32_t)gif->SHeight) < 0) {
            goto cleanup;
        }
        memcpy(frame->image.pixels, canvas, canvas_size);
        frame->delay_ms = control.DelayTime > 0 ?
                          (unsigned int)control.DelayTime * 10U : 100U;
        if (control.DisposalMode == DISPOSE_BACKGROUND) {
            fill_rect(canvas, gif->SWidth, gif->SHeight,
                      saved->ImageDesc.Left, saved->ImageDesc.Top,
                      saved->ImageDesc.Width, saved->ImageDesc.Height,
                      background);
        } else if (control.DisposalMode == DISPOSE_PREVIOUS) {
            memcpy(canvas, previous, canvas_size);
        }
    }
    result = 0;

cleanup:
    free(previous);
    free(canvas);
    if (gif != NULL) {
        DGifCloseFile(gif, &gif_error);
    }
    if (result < 0) {
        gif_animation_close(animation);
    }
    return result;
}

/**
 * @brief 从第一帧开始计时播放。
 * @param animation 已打开的动画。
 * @param now_ms 当前单调时钟毫秒值。
 */
void gif_animation_reset(struct gif_animation *animation, uint64_t now_ms)
{
    if (animation == NULL || animation->frame_count == 0) {
        return;
    }
    animation->current = 0;
    animation->completed_loops = 0;
    animation->finished = 0;
    animation->next_frame_ms = now_ms + animation->frames[0].delay_ms;
}

/**
 * @brief 按当前时间推进动画。
 * @param animation 已打开的动画。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 帧发生变化返回 1，否则返回 0。
 */
int gif_animation_advance(struct gif_animation *animation, uint64_t now_ms)
{
    int changed = 0;

    if (animation == NULL || animation->frame_count < 2 ||
        animation->finished) {
        return 0;
    }
    while (!animation->finished && now_ms >= animation->next_frame_ms) {
        if (animation->current + 1U < animation->frame_count) {
            animation->current++;
        } else if (animation->loop_count == 0 ||
                   animation->completed_loops + 1U < animation->loop_count) {
            animation->completed_loops++;
            animation->current = 0;
        } else {
            animation->finished = 1;
            break;
        }
        animation->next_frame_ms +=
            animation->frames[animation->current].delay_ms;
        changed = 1;
    }
    return changed;
}

/**
 * @brief 获取当前显示帧。
 * @param animation 已打开的动画。
 * @return 当前帧图片，无帧返回 NULL。
 */
const struct image_data *gif_animation_current(
    const struct gif_animation *animation)
{
    if (animation == NULL || animation->frames == NULL ||
        animation->current >= animation->frame_count) {
        return NULL;
    }
    return &animation->frames[animation->current].image;
}

/**
 * @brief 计算下一帧之前可等待的时间。
 * @param animation 已打开的动画。
 * @param now_ms 当前单调时钟毫秒值。
 * @param maximum_ms 调用者允许的最长等待时间。
 * @return 等待毫秒数。
 */
int gif_animation_timeout(const struct gif_animation *animation,
                          uint64_t now_ms, int maximum_ms)
{
    uint64_t remaining;

    if (animation == NULL || animation->frame_count < 2 ||
        animation->finished || animation->next_frame_ms <= now_ms) {
        return animation != NULL && animation->next_frame_ms <= now_ms ?
               0 : maximum_ms;
    }
    remaining = animation->next_frame_ms - now_ms;
    return remaining < (uint64_t)maximum_ms ? (int)remaining : maximum_ms;
}

/**
 * @brief 释放 GIF 动画全部帧。
 * @param animation 动画对象。
 */
void gif_animation_close(struct gif_animation *animation)
{
    size_t index;

    if (animation == NULL) {
        return;
    }
    for (index = 0; index < animation->frame_count; index++) {
        image_data_destroy(&animation->frames[index].image);
    }
    free(animation->frames);
    memset(animation, 0, sizeof(*animation));
}
