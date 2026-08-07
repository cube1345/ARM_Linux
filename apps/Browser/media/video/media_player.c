#include "media_player.h"

#include "browser_log.h"

#include <alsa/asoundlib.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct ffmpeg_context {
    AVFormatContext *format;
    AVCodecContext *video;
    AVCodecContext *audio;
    int video_stream;
    int audio_stream;
    struct SwsContext *sws;
    struct SwrContext *swr;
    AVFrame *frame;
    AVFrame *rgb;
    uint8_t *rgb_data;
    int rgb_linesize;
    snd_pcm_t *pcm;
    int output_rate;
    int64_t first_video_pts_ms;
    uint64_t video_wall_start_ms;
    int video_clock_started;
};

/**
 * @brief 获取播放器内部单调时钟毫秒值。
 * @return 单调时钟毫秒值。
 */
static uint64_t player_monotonic_ms(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) < 0) {
        return 0;
    }
    return (uint64_t)value.tv_sec * 1000U +
           (uint64_t)value.tv_nsec / 1000000U;
}

static int wait_ready(struct media_player *player)
{
    int stop;

    pthread_mutex_lock(&player->mutex);
    while (player->state == MEDIA_PLAYER_PAUSED &&
           !player->stop_requested && player->seek_ms < 0) {
        pthread_cond_wait(&player->condition, &player->mutex);
    }
    stop = player->stop_requested;
    pthread_mutex_unlock(&player->mutex);
    return stop;
}

static int64_t take_seek(struct media_player *player)
{
    int64_t seek;

    pthread_mutex_lock(&player->mutex);
    seek = player->seek_ms;
    player->seek_ms = -1;
    pthread_mutex_unlock(&player->mutex);
    return seek;
}

static int player_stopped(struct media_player *player)
{
    int result;

    pthread_mutex_lock(&player->mutex);
    result = player->stop_requested;
    pthread_mutex_unlock(&player->mutex);
    return result;
}

static void set_state(struct media_player *player,
                      enum media_player_state state)
{
    pthread_mutex_lock(&player->mutex);
    player->state = state;
    pthread_cond_broadcast(&player->condition);
    pthread_mutex_unlock(&player->mutex);
}

static void publish_frame(struct media_player *player, struct image_data *frame)
{
    pthread_mutex_lock(&player->mutex);
    image_data_destroy(&player->frame);
    player->frame = *frame;
    memset(frame, 0, sizeof(*frame));
    player->frame_serial++;
    pthread_mutex_unlock(&player->mutex);
}

static void set_position(struct media_player *player, uint64_t position_ms)
{
    pthread_mutex_lock(&player->mutex);
    player->position_ms = position_ms > player->duration_ms &&
                                  player->duration_ms > 0 ?
                              player->duration_ms : position_ms;
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 根据视频 PTS 控制纯视频和音视频文件的显示节奏。
 * @param player 播放器上下文。
 * @param context FFmpeg 解码上下文。
 * @param pts_ms 当前视频帧时间戳。
 */
static void pace_video_frame(struct media_player *player,
                             struct ffmpeg_context *context,
                             int64_t pts_ms)
{
    uint64_t target_ms;

    if (!context->video_clock_started) {
        context->first_video_pts_ms = pts_ms;
        context->video_wall_start_ms = player_monotonic_ms();
        context->video_clock_started = 1;
        return;
    }
    if (pts_ms <= context->first_video_pts_ms) {
        return;
    }
    target_ms = context->video_wall_start_ms +
                (uint64_t)(pts_ms - context->first_video_pts_ms);
    while (!player_stopped(player)) {
        uint64_t now_ms;
        uint64_t wait_ms;

        if (wait_ready(player)) {
            return;
        }
        now_ms = player_monotonic_ms();
        if (now_ms >= target_ms) {
            return;
        }
        wait_ms = target_ms - now_ms;
        if (wait_ms > 10U) {
            wait_ms = 10U;
        }
        usleep((useconds_t)(wait_ms * 1000U));
    }
}

static int open_codec(AVFormatContext *format, enum AVMediaType media_type,
                      int *stream_index, AVCodecContext **output)
{
    AVCodec *codec;
    AVCodecParameters *parameters;

    *stream_index = av_find_best_stream(format, media_type, -1, -1, &codec, 0);
    if (*stream_index < 0) {
        return 0;
    }
    parameters = format->streams[*stream_index]->codecpar;
    *output = avcodec_alloc_context3(codec);
    if (*output == NULL || avcodec_parameters_to_context(*output, parameters) < 0 ||
        avcodec_open2(*output, codec, NULL) < 0) {
        avcodec_free_context(output);
        *stream_index = -1;
        return -1;
    }
    return 1;
}

static void close_context(struct ffmpeg_context *context)
{
    if (context->pcm != NULL) {
        snd_pcm_drain(context->pcm);
        snd_pcm_close(context->pcm);
    }
    swr_free(&context->swr);
    sws_freeContext(context->sws);
    av_frame_free(&context->rgb);
    av_frame_free(&context->frame);
    av_free(context->rgb_data);
    avcodec_free_context(&context->video);
    avcodec_free_context(&context->audio);
    avformat_close_input(&context->format);
}

static int open_context(struct media_player *player,
                        struct ffmpeg_context *context)
{
    int result;
    AVStream *stream;

    memset(context, 0, sizeof(*context));
    context->video_stream = -1;
    context->audio_stream = -1;
    if (avformat_open_input(&context->format, player->path, NULL, NULL) < 0 ||
        avformat_find_stream_info(context->format, NULL) < 0) {
        close_context(context);
        return -1;
    }
    result = open_codec(context->format, AVMEDIA_TYPE_VIDEO,
                        &context->video_stream, &context->video);
    if (result < 0) {
        close_context(context);
        return -1;
    }
    result = open_codec(context->format, AVMEDIA_TYPE_AUDIO,
                        &context->audio_stream, &context->audio);
    if (result < 0) {
        close_context(context);
        return -1;
    }
    if (context->video_stream < 0 && context->audio_stream < 0) {
        close_context(context);
        errno = ENOTSUP;
        return -1;
    }
    context->frame = av_frame_alloc();
    if (context->frame == NULL) {
        close_context(context);
        return -1;
    }
    if (context->video != NULL) {
        stream = context->format->streams[context->video_stream];
        pthread_mutex_lock(&player->mutex);
        player->media_width = (uint32_t)context->video->width;
        player->media_height = (uint32_t)context->video->height;
        player->frame_rate = av_q2d(stream->avg_frame_rate);
        pthread_mutex_unlock(&player->mutex);
        context->sws = sws_getContext(context->video->width, context->video->height,
                                      context->video->pix_fmt, context->video->width,
                                      context->video->height, AV_PIX_FMT_RGB24,
                                      SWS_BILINEAR, NULL, NULL, NULL);
        context->rgb = av_frame_alloc();
        if (context->sws == NULL || context->rgb == NULL) {
            close_context(context);
            return -1;
        }
        context->rgb_linesize = context->video->width * 3;
        context->rgb_data = av_malloc((size_t)context->rgb_linesize *
                                      (size_t)context->video->height);
        if (context->rgb_data == NULL ||
            av_image_fill_arrays(context->rgb->data, context->rgb->linesize,
                                 context->rgb_data, AV_PIX_FMT_RGB24,
                                 context->video->width, context->video->height,
                                 1) < 0) {
            close_context(context);
            return -1;
        }
        player->duration_ms = context->format->duration > 0 ?
            (uint64_t)context->format->duration / 1000U : 0;
        (void)stream;
    } else if (context->format->duration > 0) {
        player->duration_ms = (uint64_t)context->format->duration / 1000U;
    }
    if (context->audio != NULL) {
        int channels = 2;
        uint64_t layout = context->audio->channel_layout;

        if (layout == 0) {
            layout = (uint64_t)av_get_default_channel_layout(context->audio->channels);
        }
        context->output_rate = context->audio->sample_rate;
        context->swr = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO,
                                           AV_SAMPLE_FMT_S16,
                                           context->output_rate, layout,
                                           context->audio->sample_fmt,
                                           context->audio->sample_rate, 0, NULL);
        if (context->swr == NULL || swr_init(context->swr) < 0 ||
            snd_pcm_open(&context->pcm, player->device,
                         SND_PCM_STREAM_PLAYBACK, 0) < 0 ||
            snd_pcm_set_params(context->pcm, SND_PCM_FORMAT_S16_LE,
                               SND_PCM_ACCESS_RW_INTERLEAVED, channels,
                               (unsigned int)context->output_rate, 1, 200000) < 0) {
            browser_log(BROWSER_LOG_WARN, "audio stream disabled for %s",
                        player->path);
            swr_free(&context->swr);
            if (context->pcm != NULL) {
                snd_pcm_close(context->pcm);
                context->pcm = NULL;
            }
        }
    }
    return 0;
}

static void publish_video_frame(struct media_player *player,
                                struct ffmpeg_context *context)
{
    struct image_data output;
    int y;

    memset(&output, 0, sizeof(output));
    if (image_data_create(&output, (uint32_t)context->video->width,
                          (uint32_t)context->video->height) < 0) {
        return;
    }
    for (y = 0; y < context->video->height; y++) {
        memcpy(output.pixels + (size_t)y * output.line_length,
               context->rgb_data + (size_t)y * (size_t)context->rgb_linesize,
               output.line_length);
    }
    publish_frame(player, &output);
}

static void decode_video(struct media_player *player,
                         struct ffmpeg_context *context, AVPacket *packet)
{
    int result;

    if (avcodec_send_packet(context->video, packet) < 0) {
        return;
    }
    while ((result = avcodec_receive_frame(context->video, context->frame)) >= 0) {
        struct AVRational time_base = context->format->streams[
            context->video_stream]->time_base;
        int64_t timestamp = context->frame->best_effort_timestamp;
        int64_t timestamp_ms = timestamp == AV_NOPTS_VALUE ? -1 :
            av_rescale_q(timestamp, time_base, (AVRational){1, 1000});

        if (timestamp_ms >= 0) {
            pace_video_frame(player, context, timestamp_ms);
        }
        sws_scale(context->sws, (const uint8_t * const *)context->frame->data,
                  context->frame->linesize, 0, context->video->height,
                  context->rgb->data, context->rgb->linesize);
        publish_video_frame(player, context);
        if (timestamp_ms >= 0) {
            set_position(player, (uint64_t)timestamp_ms);
        }
    }
}

static void decode_audio(struct media_player *player,
                         struct ffmpeg_context *context, AVPacket *packet)
{
    int result;

    if (avcodec_send_packet(context->audio, packet) < 0) {
        return;
    }
    while ((result = avcodec_receive_frame(context->audio, context->frame)) >= 0) {
        int samples = av_rescale_rnd(swr_get_delay(context->swr,
                                      context->audio->sample_rate) +
                                     context->frame->nb_samples,
                                     context->output_rate,
                                     context->audio->sample_rate, AV_ROUND_UP);
        uint8_t *output = NULL;
        int converted;

        if (av_samples_alloc(&output, NULL, 2, samples,
                             AV_SAMPLE_FMT_S16, 0) < 0) {
            return;
        }
        converted = swr_convert(context->swr, &output, samples,
                                (const uint8_t **)context->frame->extended_data,
                                context->frame->nb_samples);
        if (converted > 0 && context->pcm != NULL) {
            int volume;
            size_t count = (size_t)converted * 2U;
            size_t index;

            pthread_mutex_lock(&player->mutex);
            volume = player->volume;
            pthread_mutex_unlock(&player->mutex);
            for (index = 0; index < count; index++) {
                int16_t *samples16 = (int16_t *)output;
                samples16[index] = (int16_t)((int)samples16[index] * volume / 100);
            }
            (void)snd_pcm_writei(context->pcm, output, (snd_pcm_uframes_t)converted);
        }
        av_freep(&output);
    }
}

static void *media_player_thread(void *argument)
{
    struct media_player *player = argument;
    struct ffmpeg_context context;
    AVPacket packet;
    int result;

    if (open_context(player, &context) < 0) {
        set_state(player, MEDIA_PLAYER_ERROR);
        return NULL;
    }
    pthread_mutex_lock(&player->mutex);
    player->state = MEDIA_PLAYER_PLAYING;
    player->duration_ms = context.format->duration > 0 ?
        (uint64_t)context.format->duration / 1000U : player->duration_ms;
    pthread_mutex_unlock(&player->mutex);
    memset(&packet, 0, sizeof(packet));
    while (!player_stopped(player)) {
        int64_t seek = take_seek(player);

        if (seek >= 0) {
            int64_t target = seek * 1000;
            if (av_seek_frame(context.format, -1, target, AVSEEK_FLAG_BACKWARD) >= 0) {
                if (context.video != NULL) {
                    avcodec_flush_buffers(context.video);
                }
                if (context.audio != NULL) {
                    avcodec_flush_buffers(context.audio);
                }
                if (context.pcm != NULL) {
                    snd_pcm_drop(context.pcm);
                }
                context.video_clock_started = 0;
                set_position(player, (uint64_t)seek);
            }
        }
        if (wait_ready(player)) {
            break;
        }
        result = av_read_frame(context.format, &packet);
        if (result < 0) {
            set_state(player, MEDIA_PLAYER_ENDED);
            break;
        }
        if (packet.stream_index == context.video_stream && context.video != NULL) {
            decode_video(player, &context, &packet);
        } else if (packet.stream_index == context.audio_stream &&
                   context.audio != NULL && context.swr != NULL) {
            decode_audio(player, &context, &packet);
        }
        av_packet_unref(&packet);
    }
    av_packet_unref(&packet);
    close_context(&context);
    return NULL;
}

int media_player_init(struct media_player *player)
{
    if (player == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(player, 0, sizeof(*player));
    if (pthread_mutex_init(&player->mutex, NULL) != 0) {
        errno = ENOMEM;
        return -1;
    }
    if (pthread_cond_init(&player->condition, NULL) != 0) {
        pthread_mutex_destroy(&player->mutex);
        errno = ENOMEM;
        return -1;
    }
    player->volume = 80;
    player->seek_ms = -1;
    player->state = MEDIA_PLAYER_STOPPED;
    player->initialized = 1;
    return 0;
}

int media_player_start(struct media_player *player, const char *path,
                       const char *device)
{
    if (player == NULL || path == NULL || !player->initialized) {
        errno = EINVAL;
        return -1;
    }
    media_player_stop(player);
    snprintf(player->path, sizeof(player->path), "%s", path);
    snprintf(player->device, sizeof(player->device), "%s",
             device != NULL && device[0] != '\0' ? device : "default");
    pthread_mutex_lock(&player->mutex);
    image_data_destroy(&player->frame);
    player->state = MEDIA_PLAYER_PLAYING;
    player->position_ms = 0;
    player->duration_ms = 0;
    player->frame_serial = 0;
    player->media_width = 0;
    player->media_height = 0;
    player->frame_rate = 0.0;
    player->seek_ms = -1;
    player->stop_requested = 0;
    pthread_mutex_unlock(&player->mutex);
    if (pthread_create(&player->thread, NULL, media_player_thread, player) != 0) {
        set_state(player, MEDIA_PLAYER_ERROR);
        return -1;
    }
    player->thread_created = 1;
    return 0;
}

void media_player_toggle_pause(struct media_player *player)
{
    if (player == NULL) {
        return;
    }
    pthread_mutex_lock(&player->mutex);
    if (player->state == MEDIA_PLAYER_PLAYING) {
        player->state = MEDIA_PLAYER_PAUSED;
    } else if (player->state == MEDIA_PLAYER_PAUSED) {
        player->state = MEDIA_PLAYER_PLAYING;
    }
    pthread_cond_broadcast(&player->condition);
    pthread_mutex_unlock(&player->mutex);
}

void media_player_set_volume(struct media_player *player, int volume)
{
    if (player == NULL) {
        return;
    }
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    pthread_mutex_lock(&player->mutex);
    player->volume = volume;
    pthread_mutex_unlock(&player->mutex);
}

void media_player_seek_percent(struct media_player *player, int percent)
{
    uint64_t duration;

    if (player == NULL) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    pthread_mutex_lock(&player->mutex);
    duration = player->duration_ms;
    player->seek_ms = duration > 0 ? (int64_t)(duration * (uint64_t)percent / 100U) : 0;
    pthread_cond_broadcast(&player->condition);
    pthread_mutex_unlock(&player->mutex);
}

/**
 * @brief 请求跳转到指定毫秒位置。
 * @param player 播放器上下文。
 * @param position_ms 目标位置，自动限制到媒体时长。
 */
void media_player_seek_ms(struct media_player *player, int64_t position_ms)
{
    uint64_t duration;

    if (player == NULL) return;
    pthread_mutex_lock(&player->mutex);
    duration = player->duration_ms;
    if (position_ms < 0) position_ms = 0;
    if (duration > 0 && (uint64_t)position_ms > duration) {
        position_ms = (int64_t)duration;
    }
    player->seek_ms = position_ms;
    pthread_cond_broadcast(&player->condition);
    pthread_mutex_unlock(&player->mutex);
}

void media_player_get_status(struct media_player *player,
                             struct media_player_status *status)
{
    if (player == NULL || status == NULL) return;
    pthread_mutex_lock(&player->mutex);
    status->state = player->state;
    status->volume = player->volume;
    status->position_ms = player->position_ms;
    status->duration_ms = player->duration_ms;
    status->frame_serial = player->frame_serial;
    status->width = player->media_width;
    status->height = player->media_height;
    status->frame_rate = player->frame_rate;
    status->has_video = player->frame.pixels != NULL;
    pthread_mutex_unlock(&player->mutex);
}

int media_player_copy_frame(struct media_player *player,
                            struct image_data *output, uint64_t *serial)
{
    int result = 0;

    if (player == NULL || output == NULL) return 0;
    pthread_mutex_lock(&player->mutex);
    if (player->frame.pixels != NULL && player->frame.width > 0) {
        if (image_data_create(output, player->frame.width, player->frame.height) == 0) {
            memcpy(output->pixels, player->frame.pixels, player->frame.size);
            if (serial != NULL) *serial = player->frame_serial;
            result = 1;
        }
    }
    pthread_mutex_unlock(&player->mutex);
    return result;
}

void media_player_stop(struct media_player *player)
{
    if (player == NULL || !player->initialized) return;
    pthread_mutex_lock(&player->mutex);
    player->stop_requested = 1;
    pthread_cond_broadcast(&player->condition);
    pthread_mutex_unlock(&player->mutex);
    if (player->thread_created) {
        pthread_join(player->thread, NULL);
        player->thread_created = 0;
    }
    pthread_mutex_lock(&player->mutex);
    player->state = MEDIA_PLAYER_STOPPED;
    player->stop_requested = 0;
    pthread_mutex_unlock(&player->mutex);
}

const char *media_player_state_name(enum media_player_state state)
{
    switch (state) {
    case MEDIA_PLAYER_PLAYING: return "Playing";
    case MEDIA_PLAYER_PAUSED: return "Paused";
    case MEDIA_PLAYER_ENDED: return "Ended";
    case MEDIA_PLAYER_ERROR: return "Error";
    case MEDIA_PLAYER_STOPPED:
    default: return "Stopped";
    }
}

void media_player_destroy(struct media_player *player)
{
    if (player == NULL || !player->initialized) return;
    media_player_stop(player);
    image_data_destroy(&player->frame);
    pthread_cond_destroy(&player->condition);
    pthread_mutex_destroy(&player->mutex);
    memset(player, 0, sizeof(*player));
}
