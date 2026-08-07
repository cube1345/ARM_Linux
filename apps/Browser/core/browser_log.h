#ifndef BROWSER_LOG_H
#define BROWSER_LOG_H

/** @brief 浏览器日志等级。 */
enum browser_log_level {
    BROWSER_LOG_ERROR = 0,
    BROWSER_LOG_WARN,
    BROWSER_LOG_INFO,
    BROWSER_LOG_DEBUG,
    BROWSER_LOG_QUIET
};

/**
 * @brief 从环境变量 BROWSER_LOG_LEVEL 初始化日志等级。
 */
void browser_log_init_from_env(void);

/**
 * @brief 设置当前日志等级。
 * @param level 日志等级。
 */
void browser_log_set_level(enum browser_log_level level);

/**
 * @brief 输出格式化日志。
 * @param level 日志等级。
 * @param format printf 风格格式串。
 */
void browser_log(enum browser_log_level level, const char *format, ...);

/**
 * @brief 输出带 errno 文本的日志。
 * @param level 日志等级。
 * @param context 错误上下文。
 */
void browser_log_errno(enum browser_log_level level, const char *context);

#endif
