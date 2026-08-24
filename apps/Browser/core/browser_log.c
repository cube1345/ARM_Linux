#include "browser_log.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/** @brief 当前最低输出日志等级。 */
static enum browser_log_level current_level = BROWSER_LOG_INFO;
static char crash_log_path[PATH_MAX];

#define BROWSER_CRASH_LOG_DEFAULT "/var/log/media-browser-crash.log"

/** @brief 将无符号整数追加到信号日志缓冲区。 */
static size_t append_signal_number(char *buffer, size_t offset,
                                   size_t capacity, unsigned int value)
{
    char digits[10];
    size_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));
    while (count > 0 && offset + 1U < capacity) {
        buffer[offset++] = digits[--count];
    }
    return offset;
}

/** @brief 在致命信号上下文中追加一行最小崩溃日志。 */
static void write_crash_log(int signal_number)
{
    static const char prefix[] = "media-browser fatal signal=";
    static const char suffix[] = "\n";
    char buffer[64];
    size_t offset = 0;
    size_t index;
    size_t written = 0;
    int fd;

    if (crash_log_path[0] == '\0') return;
    for (index = 0; index < sizeof(prefix) - 1U; index++) {
        buffer[offset++] = prefix[index];
    }
    offset = append_signal_number(buffer, offset, sizeof(buffer),
                                  (unsigned int)signal_number);
    if (offset + sizeof(suffix) - 1U >= sizeof(buffer)) return;
    for (index = 0; index < sizeof(suffix) - 1U; index++) {
        buffer[offset++] = suffix[index];
    }
    fd = open(crash_log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) return;
    while (written < offset) {
        ssize_t count = write(fd, buffer + written, offset - written);

        if (count > 0) {
            written += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    (void)close(fd);
}

/** @brief 处理致命信号并在默认动作前写入崩溃日志。 */
static void handle_crash_signal(int signal_number)
{
    int saved_errno = errno;

    write_crash_log(signal_number);
    (void)signal(signal_number, SIG_DFL);
    (void)raise(signal_number);
    errno = saved_errno;
    _exit(128 + signal_number);
}

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

/** @brief 安装致命信号持久化 handler。 */
int browser_log_install_crash_handler(const char *path)
{
    static const int signals[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV};
    struct sigaction action;
    const char *selected_path = path;
    size_t index;

    if (selected_path == NULL || selected_path[0] == '\0') {
        selected_path = BROWSER_CRASH_LOG_DEFAULT;
    }
    if (snprintf(crash_log_path, sizeof(crash_log_path), "%s",
                 selected_path) >= (int)sizeof(crash_log_path)) {
        errno = ENAMETOOLONG;
        crash_log_path[0] = '\0';
        return -1;
    }
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_crash_signal;
    sigemptyset(&action.sa_mask);
    for (index = 0; index < sizeof(signals) / sizeof(signals[0]); index++) {
        if (sigaction(signals[index], &action, NULL) < 0) return -1;
    }
    return 0;
}
