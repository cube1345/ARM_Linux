#include "browser_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BROWSER_CONFIG_DEFAULT_PATH "/etc/media-browser.conf"
#define BROWSER_CONFIG_ENV "BROWSER_CONFIG_PATH"

/** @brief 查找路径在列表中的索引。 */
static int path_list_index(const struct browser_path_list *list,
                           const char *path)
{
    size_t index;

    if (list == NULL || path == NULL || path[0] == '\0') return -1;
    for (index = 0; index < list->count; index++) {
        if (strcmp(list->paths[index], path) == 0) return (int)index;
    }
    return -1;
}

/** @brief 判断路径列表是否包含指定路径。 */
int browser_path_list_contains(const struct browser_path_list *list,
                               const char *path)
{
    return path_list_index(list, path) >= 0;
}

/** @brief 追加路径到列表尾部，超出容量时忽略。 */
void browser_path_list_append(struct browser_path_list *list,
                              const char *path)
{
    if (list == NULL || path == NULL || path[0] == '\0' ||
        list->count >= BROWSER_PATH_LIST_LIMIT ||
        browser_path_list_contains(list, path)) {
        return;
    }
    snprintf(list->paths[list->count], sizeof(list->paths[list->count]),
             "%s", path);
    list->count++;
}

/** @brief 从路径列表移除指定路径。 */
int browser_path_list_remove(struct browser_path_list *list,
                             const char *path)
{
    int found = path_list_index(list, path);
    size_t index;

    if (found < 0) return 0;
    for (index = (size_t)found; index + 1U < list->count; index++) {
        snprintf(list->paths[index], sizeof(list->paths[index]), "%s",
                 list->paths[index + 1U]);
    }
    list->count--;
    if (list->count < BROWSER_PATH_LIST_LIMIT) {
        list->paths[list->count][0] = '\0';
    }
    return 1;
}

/** @brief 将路径移动到列表最前方，超出容量时丢弃最旧项。 */
void browser_path_list_add_front(struct browser_path_list *list,
                                 const char *path)
{
    size_t index;
    size_t limit;

    if (list == NULL || path == NULL || path[0] == '\0') return;
    (void)browser_path_list_remove(list, path);
    limit = list->count < BROWSER_PATH_LIST_LIMIT - 1U ? list->count :
            BROWSER_PATH_LIST_LIMIT - 1U;
    for (index = limit; index > 0; index--) {
        snprintf(list->paths[index], sizeof(list->paths[index]), "%s",
                 list->paths[index - 1U]);
    }
    snprintf(list->paths[0], sizeof(list->paths[0]), "%s", path);
    list->count = limit + 1U;
}

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

    if (*value == '-') return -1;
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *trim(end) != '\0') return -1;
    *output = parsed;
    return 0;
}

/** @brief 解析一个 64 位非负整数。 */
static int parse_uint64(const char *value, uint64_t *output)
{
    char *end;
    unsigned long long parsed;

    if (*value == '-') return -1;
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || *trim(end) != '\0') return -1;
    *output = (uint64_t)parsed;
    return 0;
}

/** @brief 初始化默认设置。 */
void browser_config_defaults(struct browser_config *config)
{
    if (config == NULL) return;
    config->font_size = 24U;
    config->volume = 80;
    config->file_sort = FILE_LIST_SORT_NAME;
    config->playback_mode = BROWSER_PLAYBACK_REPEAT_ALL;
    config->resume_path[0] = '\0';
    config->resume_position_ms = 0;
    config->recent_files.count = 0;
    config->favorite_files.count = 0;
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
    } else if (strcmp(key, "playback_mode") == 0 &&
               parse_unsigned(value, &parsed) == 0 && parsed <= 3U) {
        config->playback_mode = (enum browser_playback_mode)parsed;
    } else if (strcmp(key, "resume_path") == 0) {
        snprintf(config->resume_path, sizeof(config->resume_path), "%s",
                 value);
    } else if (strcmp(key, "resume_position_ms") == 0) {
        (void)parse_uint64(value, &config->resume_position_ms);
    } else if (strcmp(key, "recent") == 0) {
        browser_path_list_append(&config->recent_files, value);
    } else if (strcmp(key, "favorite") == 0) {
        browser_path_list_append(&config->favorite_files, value);
    }
}

/** @brief 从文本配置文件读取设置。 */
int browser_config_load(const char *path, struct browser_config *config)
{
    FILE *stream;
    char line[PATH_MAX + 64];

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
    size_t index;

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
    if (fprintf(stream, "# media-browser settings\nfont_size=%u\nvolume=%d\nsort=%d\nplayback_mode=%d\nresume_path=%s\nresume_position_ms=%llu\n",
                config->font_size, config->volume, (int)config->file_sort,
                (int)config->playback_mode, config->resume_path,
                (unsigned long long)config->resume_position_ms) < 0 ||
        fflush(stream) != 0) {
        fclose(stream);
        remove(temporary);
        return -1;
    }
    for (index = 0; index < config->recent_files.count; index++) {
        if (fprintf(stream, "recent=%s\n",
                    config->recent_files.paths[index]) < 0) {
            fclose(stream);
            remove(temporary);
            return -1;
        }
    }
    for (index = 0; index < config->favorite_files.count; index++) {
        if (fprintf(stream, "favorite=%s\n",
                    config->favorite_files.paths[index]) < 0) {
            fclose(stream);
            remove(temporary);
            return -1;
        }
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
