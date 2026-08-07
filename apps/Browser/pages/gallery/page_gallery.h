#ifndef PAGE_GALLERY_H
#define PAGE_GALLERY_H

#include "browser_app.h"
#include "input_keyboard.h"

/** @brief 绘制 Gallery 缩略图网格。 */
int render_gallery_page(struct browser_app *app);

/** @brief 处理 Gallery 键盘动作。 */
int handle_gallery_key(struct browser_app *app, enum input_action action);

/** @brief 处理 Gallery 触摸动作。 */
int handle_gallery_touch(struct browser_app *app,
                         const struct browser_input *input);

/** @brief 释放 Gallery 全部缩略图缓存。 */
void gallery_cache_clear(struct browser_app *app);

#endif
