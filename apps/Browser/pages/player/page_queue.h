#ifndef PAGE_QUEUE_H
#define PAGE_QUEUE_H

#include "browser_app.h"

/** @brief 判断文件类型是否属于某个播放队列。 */
typedef int (*page_queue_match_fn)(enum file_type type);

/**
 * @brief 绘制当前目录中的播放队列摘要。
 * @param app 浏览器上下文。
 * @param match 判断条目是否属于该队列的函数。
 * @param title 队列标题。
 * @param x 面板左上角 X。
 * @param y 面板左上角 Y。
 * @param width 面板宽度。
 * @param height 面板高度。
 */
void page_queue_draw(struct browser_app *app, page_queue_match_fn match,
                     const char *title, int x, int y, int width,
                     int height);

#endif
