#include "jpeg_decoder.h"

#include "browser_log.h"

#include <errno.h>
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdint.h>

/** @brief libjpeg 错误恢复上下文。 */
struct jpeg_error_context {
    struct jpeg_error_mgr manager;
    jmp_buf jump_buffer;
};

/**
 * @brief 将 libjpeg 致命错误转换为可恢复跳转。
 *
 * @param common libjpeg 公共对象。
 */
static void jpeg_error_exit(j_common_ptr common)
{
    struct jpeg_error_context *error = (struct jpeg_error_context *)common->err;

    (*common->err->output_message)(common);
    longjmp(error->jump_buffer, 1);
}

/**
 * @brief 将 JPEG 文件解码成 RGB888 图片。
 *
 * @param path JPEG 文件路径。
 * @param image 输出图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int jpeg_decode(const char *path, struct image_data *image)
{
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_context error;
    FILE *file = NULL;
    volatile int created = 0;
    volatile int result = -1;

    if (path == NULL || image == NULL || image->pixels != NULL) {
        errno = EINVAL;
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        browser_log_errno(BROWSER_LOG_ERROR, path);
        return -1;
    }
    decoder.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = jpeg_error_exit;
    if (setjmp(error.jump_buffer) != 0) {
        errno = EINVAL;
        goto cleanup;
    }
    jpeg_create_decompress(&decoder);
    created = 1;
    jpeg_stdio_src(&decoder, file);
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    if (decoder.output_components != 3 ||
        image_data_create(image, decoder.output_width,
                          decoder.output_height) < 0) {
        goto cleanup;
    }
    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW row = image->pixels +
                       (size_t)decoder.output_scanline * image->line_length;

        if (jpeg_read_scanlines(&decoder, &row, 1) != 1) {
            errno = EIO;
            goto cleanup;
        }
    }
    jpeg_finish_decompress(&decoder);
    result = 0;

cleanup:
    if (created) {
        jpeg_destroy_decompress(&decoder);
    }
    if (fclose(file) != 0 && result == 0) {
        result = -1;
    }
    if (result < 0) {
        image_data_destroy(image);
    }
    return (int)result;
}
