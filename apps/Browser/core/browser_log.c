#include "browser_log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/** @brief 当前最低输出日志等级。 */
static enum browser_log_level current_level = BROWSER_LOG_INFO;

/**
 * @brief 获取日志等级名称。
 * @param level 日志等级。
 * @return 静态等级名称。
 */
static const char *browser_log_level_name(enum browser_log_level level)
{
    switch (level) {
    case BROWSER_LOG_ERROR: return "ERROR";
    case BROWSER_LOG_WARN: return "WARN";
    case BROWSER_LOG_INFO: return "INFO";
    case BROWSER_LOG_DEBUG: return "DEBUG";
    case BROWSER_LOG_QUIET:
    default: return "QUIET";
    }
}

/**
 * @brief 解析日志等级字符串。
 * @param text 日志等级字符串。
 * @param output 输出日志等级。
 * @return 解析成功返回 1，否则返回 0。
 */
static int parse_log_level(const char *text, enum browser_log_level *output)
{
    if (text == NULL || output == NULL) {
        return 0;
    }
    if (strcasecmp(text, "error") == 0) {
        *output = BROWSER_LOG_ERROR;
        return 1;
    }
    if (strcasecmp(text, "warn") == 0 ||
        strcasecmp(text, "warning") == 0) {
        *output = BROWSER_LOG_WARN;
        return 1;
    }
    if (strcasecmp(text, "info") == 0) {
        *output = BROWSER_LOG_INFO;
        return 1;
    }
    if (strcasecmp(text, "debug") == 0) {
        *output = BROWSER_LOG_DEBUG;
        return 1;
    }
    if (strcasecmp(text, "quiet") == 0) {
        *output = BROWSER_LOG_QUIET;
        return 1;
    }
    return 0;
}

/**
 * @brief 从环境变量 BROWSER_LOG_LEVEL 初始化日志等级。
 */
void browser_log_init_from_env(void)
{
    enum browser_log_level level;

    if (parse_log_level(getenv("BROWSER_LOG_LEVEL"), &level)) {
        current_level = level;
    }
}

/**
 * @brief 设置当前日志等级。
 * @param level 日志等级。
 */
void browser_log_set_level(enum browser_log_level level)
{
    current_level = level;
}

/**
 * @brief 输出格式化日志。
 * @param level 日志等级。
 * @param format printf 风格格式串。
 */
void browser_log(enum browser_log_level level, const char *format, ...)
{
    va_list arguments;

    if (format == NULL || current_level == BROWSER_LOG_QUIET ||
        level > current_level) {
        return;
    }
    fprintf(stderr, "[browser] %s: ", browser_log_level_name(level));
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

/**
 * @brief 输出带 errno 文本的日志。
 * @param level 日志等级。
 * @param context 错误上下文。
 */
void browser_log_errno(enum browser_log_level level, const char *context)
{
    int saved_errno = errno;

    browser_log(level, "%s: %s", context == NULL ? "errno" : context,
                strerror(saved_errno));
}
