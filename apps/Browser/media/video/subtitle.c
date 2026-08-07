#include "subtitle.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUBTITLE_LINE_SIZE 1024U
#define SUBTITLE_MAX_ENTRIES 4096U

/** @brief 跳过 ASCII 空白。 */
static const char *skip_spaces(const char *text)
{
    while (*text == ' ' || *text == '\t') text++;
    return text;
}

/** @brief 解析 SRT 的 HH:MM:SS,mmm 时间戳。 */
static int parse_timestamp(const char *text, uint64_t *milliseconds,
                           const char **end)
{
    char *next;
    unsigned long hours;
    unsigned long minutes;
    unsigned long seconds;
    unsigned long millis;
    const char *component;

    errno = 0;
    hours = strtoul(text, &next, 10);
    if (errno != 0 || next == text || *next++ != ':') return -1;
    component = next;
    minutes = strtoul(component, &next, 10);
    if (errno != 0 || next == component || *next++ != ':') return -1;
    component = next;
    seconds = strtoul(component, &next, 10);
    if (errno != 0 || next == component ||
        (*next != ',' && *next != '.')) return -1;
    next++;
    component = next;
    millis = strtoul(component, &next, 10);
    if (errno != 0 || next == component || minutes >= 60U || seconds >= 60U ||
        millis >= 1000U || hours > UINT64_MAX / 3600000U) {
        return -1;
    }
    *milliseconds = (((uint64_t)hours * 60U + minutes) * 60U + seconds) *
                    1000U + millis;
    *end = next;
    return 0;
}

/** @brief 解析一行 SRT 起止时间。 */
static int parse_timing(const char *line, uint64_t *start_ms,
                        uint64_t *end_ms)
{
    const char *next;

    line = skip_spaces(line);
    if (parse_timestamp(line, start_ms, &next) < 0) return -1;
    next = skip_spaces(next);
    if (strncmp(next, "-->", 3) != 0) return -1;
    next = skip_spaces(next + 3);
    if (parse_timestamp(next, end_ms, &next) < 0 || *skip_spaces(next) != '\0' ||
        *end_ms <= *start_ms) {
        return -1;
    }
    return 0;
}

/** @brief 去掉输入行尾换行符。 */
static void trim_line_end(char *line)
{
    size_t length = strlen(line);

    while (length > 0 && (line[length - 1U] == '\n' ||
                          line[length - 1U] == '\r')) {
        line[--length] = '\0';
    }
}

/** @brief 向字幕文本追加一行并用空格分隔。 */
static void append_text(char *output, const char *line)
{
    size_t length = strlen(output);
    size_t available;
    size_t copy_length;

    if (length > 0 && length + 1U < SUBTITLE_TEXT_SIZE) {
        output[length++] = ' ';
        output[length] = '\0';
    }
    available = SUBTITLE_TEXT_SIZE - length;
    if (available <= 1U) return;
    copy_length = strlen(line);
    if (copy_length >= available) copy_length = available - 1U;
    memcpy(output + length, line, copy_length);
    output[length + copy_length] = '\0';
}

/** @brief 追加一个字幕条目。 */
static int append_entry(struct subtitle_track *track, uint64_t start_ms,
                        uint64_t end_ms, const char *text)
{
    struct subtitle_entry *entries;

    if (track->count >= SUBTITLE_MAX_ENTRIES) {
        errno = EFBIG;
        return -1;
    }
    entries = realloc(track->entries,
                      (track->count + 1U) * sizeof(*entries));
    if (entries == NULL) return -1;
    track->entries = entries;
    entries[track->count].start_ms = start_ms;
    entries[track->count].end_ms = end_ms;
    snprintf(entries[track->count].text,
             sizeof(entries[track->count].text), "%s", text);
    track->count++;
    return 0;
}

/** @brief 将媒体路径后缀替换为 .srt。 */
static int sidecar_path(const char *media_path, char *output,
                        size_t output_size)
{
    const char *slash;
    char *dot;
    int written;

    written = snprintf(output, output_size, "%s", media_path);
    if (written < 0 || (size_t)written >= output_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    slash = strrchr(output, '/');
    dot = strrchr(output, '.');
    if (dot == NULL || (slash != NULL && dot < slash)) {
        dot = output + strlen(output);
    }
    if ((size_t)(dot - output) + 5U > output_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    snprintf(dot, output_size - (size_t)(dot - output), ".srt");
    return 0;
}

/** @brief 自动加载与媒体同 basename 的 .srt 字幕。 */
int subtitle_track_load_for_media(struct subtitle_track *track,
                                  const char *media_path)
{
    char path[4096];
    char line[SUBTITLE_LINE_SIZE];
    FILE *stream;
    int read_error;
    int close_result;

    if (track == NULL || media_path == NULL) {
        errno = EINVAL;
        return -1;
    }
    subtitle_track_close(track);
    if (sidecar_path(media_path, path, sizeof(path)) < 0) return -1;
    stream = fopen(path, "r");
    if (stream == NULL) return errno == ENOENT ? 0 : -1;
    while (fgets(line, sizeof(line), stream) != NULL) {
        uint64_t start_ms;
        uint64_t end_ms;
        char text[SUBTITLE_TEXT_SIZE] = {0};

        trim_line_end(line);
        if (parse_timing(line, &start_ms, &end_ms) < 0) continue;
        while (fgets(line, sizeof(line), stream) != NULL) {
            trim_line_end(line);
            if (line[0] == '\0') break;
            append_text(text, line);
        }
        if (text[0] != '\0' &&
            append_entry(track, start_ms, end_ms, text) < 0) {
            fclose(stream);
            subtitle_track_close(track);
            return -1;
        }
    }
    read_error = ferror(stream);
    close_result = fclose(stream);
    if (read_error != 0 || close_result != 0) {
        subtitle_track_close(track);
        return -1;
    }
    return 0;
}

/** @brief 查找指定播放位置应显示的字幕。 */
const char *subtitle_track_text_at(const struct subtitle_track *track,
                                   uint64_t position_ms)
{
    size_t index;

    if (track == NULL) return NULL;
    for (index = 0; index < track->count; index++) {
        if (position_ms >= track->entries[index].start_ms &&
            position_ms < track->entries[index].end_ms) {
            return track->entries[index].text;
        }
    }
    return NULL;
}

/** @brief 释放字幕集合。 */
void subtitle_track_close(struct subtitle_track *track)
{
    if (track == NULL) return;
    free(track->entries);
    track->entries = NULL;
    track->count = 0;
}
