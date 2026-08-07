#ifndef AUDIO_METADATA_H
#define AUDIO_METADATA_H

#define AUDIO_METADATA_TEXT_SIZE 128

/** @brief 音频文件可显示的文本标签。 */
struct audio_metadata {
    char title[AUDIO_METADATA_TEXT_SIZE];
    char artist[AUDIO_METADATA_TEXT_SIZE];
    char album[AUDIO_METADATA_TEXT_SIZE];
    int has_tags;
};

/**
 * @brief 读取 MP3 的 ID3v2/ID3v1 文本标签。
 * @param path 文件路径。
 * @param metadata 输出标签。
 * @return 成功或无标签返回 0，读取失败返回 -1。
 */
int audio_metadata_read(const char *path, struct audio_metadata *metadata);

#endif
