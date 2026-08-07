#include "browser_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BROWSER_CONFIG_DEFAULT_PATH "/etc/media-browser.conf"
#define BROWSER_CONFIG_ENV "BROWSER_CONFIG_PATH"

/** @brief 去掉字符串首尾空白。 */
static char *trim(char *value)
{
    char *end;

    while (*value == ' ' || *value == '\t' || *value == '\r' ||
           *value == '\n') {
        value++;
    }
    end = value + strlen(value);
    while (end > value && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return value;
}

/** @brief 解析一个非负整数。 */
static int parse_unsigned(const char *value, unsigned long *output)
{
    char *end;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *trim(end) != '\0') return -1;
    *output = parsed;
    return 0;
}

/** @brief 初始化默认设置。 */
void browser_config_defaults(struct browser_config *config)
{
    if (config == NULL) return;
    config->font_size = 24U;
    config->volume = 80;
    config->file_sort = FILE_LIST_SORT_NAME;
}

/** @brief 解析一行配置键值。 */
static void parse_line(char *line, struct browser_config *config)
{
    char *separator;
    char *key;
    char *value;
    unsigned long parsed;

    key = trim(line);
    if (*key == '\0' || *key == '#') return;
    separator = strchr(key, '=');
    if (separator == NULL) return;
    *separator = '\0';
    value = trim(separator + 1);
    key = trim(key);
    if (strcmp(key, "font_size") == 0 &&
        parse_unsigned(value, &parsed) == 0) {
        config->font_size = parsed < 18U ? 18U :
                            parsed > 34U ? 34U : (uint32_t)parsed;
    } else if (strcmp(key, "volume") == 0 &&
               parse_unsigned(value, &parsed) == 0) {
        config->volume = parsed > 100U ? 100 : (int)parsed;
    } else if (strcmp(key, "sort") == 0 &&
               parse_unsigned(value, &parsed) == 0 && parsed <= 3U) {
        config->file_sort = (enum file_list_sort)parsed;
    }
}

/** @brief 从文本配置文件读取设置。 */
int browser_config_load(const char *path, struct browser_config *config)
{
    FILE *stream;
    char line[256];

    if (path == NULL || config == NULL) {
        errno = EINVAL;
        return -1;
    }
    browser_config_defaults(config);
    stream = fopen(path, "r");
    if (stream == NULL) return errno == ENOENT ? 0 : -1;
    while (fgets(line, sizeof(line), stream) != NULL) parse_line(line, config);
    if (ferror(stream) != 0) {
        fclose(stream);
        return -1;
    }
    return fclose(stream) < 0 ? -1 : 0;
}

/** @brief 将设置原子写入文本配置文件。 */
int browser_config_save(const char *path,
                        const struct browser_config *config)
{
    char temporary[PATH_MAX];
    FILE *stream;
    int written;
    int close_result;

    if (path == NULL || config == NULL) {
        errno = EINVAL;
        return -1;
    }
    written = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    stream = fopen(temporary, "w");
    if (stream == NULL) return -1;
    if (fprintf(stream, "# media-browser settings\nfont_size=%u\nvolume=%d\nsort=%d\n",
                config->font_size, config->volume, (int)config->file_sort) < 0 ||
        fflush(stream) != 0) {
        fclose(stream);
        remove(temporary);
        return -1;
    }
    close_result = fclose(stream);
    if (close_result != 0) {
        remove(temporary);
        return -1;
    }
    if (rename(temporary, path) < 0) {
        remove(temporary);
        return -1;
    }
    return 0;
}

/** @brief 从环境变量获取配置路径。 */
int browser_config_path(char *output, size_t output_size)
{
    const char *path = getenv(BROWSER_CONFIG_ENV);
    int written;

    if (output == NULL || output_size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (path == NULL || *path == '\0') path = BROWSER_CONFIG_DEFAULT_PATH;
    written = snprintf(output, output_size, "%s", path);
    if (written < 0 || (size_t)written >= output_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}
