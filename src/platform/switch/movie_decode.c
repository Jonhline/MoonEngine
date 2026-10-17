/* Native video only. FFmpeg 7.1 custom AVIO, send/receive and filter APIs:
   https://ffmpeg.org/doxygen/7.1/avio_read_callback_8c-example.html
   https://ffmpeg.org/doxygen/7.1/decode_filter_video_8c-example.html
   This is our adapter; no SDK or upstream implementation is vendored. */
#include "movie_decode.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gt4_movie_decoder {
    const uint8_t *data;
    size_t size, offset;
    AVFormatContext *format;
    AVIOContext *io;
    AVCodecContext *codec;
    AVPacket *packet;
    AVFrame *frame, *output;
    AVFilterGraph *filters;
    AVFilterContext *source, *sink;
    unsigned decoded;
    int video, interlace, repeat, input_eof, filter_eof, ipu;
};

static int read_memory(void *user, uint8_t *out, int bytes)
{
    gt4_movie_decoder *d = user;
    size_t take = d->size - d->offset;
    if (take > (size_t)bytes)
        take = (size_t)bytes;
    if (!take)
        return AVERROR_EOF;
    memcpy(out, d->data + d->offset, take);
    d->offset += take;
    return (int)take;
}
static int64_t seek_memory(void *user, int64_t offset, int whence)
{
    gt4_movie_decoder *d = user;
    if (whence & AVSEEK_SIZE)
        return (int64_t)d->size;
    whence &= ~AVSEEK_FORCE;
    int64_t base = whence == SEEK_SET   ? 0
                   : whence == SEEK_CUR ? (int64_t)d->offset
                   : whence == SEEK_END ? (int64_t)d->size
                                        : -1;
    if (base < 0 || offset < -base || offset > (int64_t)d->size - base)
        return AVERROR(EINVAL);
    d->offset = (size_t)(base + offset);
    return (int64_t)d->offset;
}
void gt4_movie_decoder_close(gt4_movie_decoder *d)
{
    if (!d)
        return;
    avfilter_graph_free(&d->filters);
    av_frame_free(&d->frame);
    av_frame_free(&d->output);
    av_packet_free(&d->packet);
    avcodec_free_context(&d->codec);
    avformat_close_input(&d->format);
    if (d->io)
        av_freep(&d->io->buffer);
    avio_context_free(&d->io);
    free(d);
}
gt4_movie_decoder *gt4_movie_decoder_open(const uint8_t *data, size_t size, int interlace,
                                          int repeat)
{
    if (!data || !size || size > 512u * 1024 * 1024)
        return NULL;
    gt4_movie_decoder *d = calloc(1, sizeof *d);
    if (!d)
        return NULL;
    const char *stage = "AVIO";
    d->data = data;
    d->size = size;
    d->interlace = interlace != 0;
    d->repeat = repeat != 0;
    d->ipu = size >= 16 && !memcmp(data, "ipum", 4);
    uint8_t *buffer = av_malloc(32768);
    if (!buffer)
        goto fail;
    d->io = avio_alloc_context(buffer, 32768, 0, d, read_memory, NULL, seek_memory);
    if (!d->io) {
        av_free(buffer);
        goto fail;
    }
    d->format = avformat_alloc_context();
    if (!d->format)
        goto fail;
    d->format->pb = d->io;
    d->format->flags |= AVFMT_FLAG_CUSTOM_IO;
    stage = "demux";
    if (avformat_open_input(&d->format, NULL, av_find_input_format(d->ipu ? "ipu" : "mpeg"), NULL) < 0 ||
        avformat_find_stream_info(d->format, NULL) < 0)
        goto fail;
    stage = "video stream";
    d->video = av_find_best_stream(d->format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (d->video < 0)
        goto fail;
    stage = "codec id";
    AVCodecParameters *parameters = d->format->streams[d->video]->codecpar;
    if (parameters->codec_id != (d->ipu ? AV_CODEC_ID_IPU : AV_CODEC_ID_MPEG2VIDEO))
        goto fail;
    stage = "decoder";
    const AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
    d->codec = codec ? avcodec_alloc_context3(codec) : NULL;
    if (!d->codec || avcodec_parameters_to_context(d->codec, parameters) < 0)
        goto fail;
    stage = "codec open";
    d->codec->pkt_timebase = d->format->streams[d->video]->time_base;
    d->codec->thread_count = 1;
    if (avcodec_open2(d->codec, codec, NULL) < 0)
        goto fail;
    d->packet = av_packet_alloc();
    d->frame = av_frame_alloc();
    d->output = av_frame_alloc();
    if (!d->packet || !d->frame || !d->output)
        goto fail;
    return d;
fail:
    if (d->ipu)
        fprintf(stderr, "Switch IPU: open failed at %s\n", stage);
    gt4_movie_decoder_close(d);
    return NULL;
}
static int make_filters(gt4_movie_decoder *d)
{
    AVStream *stream = d->format->streams[d->video];
    AVRational rate = av_guess_frame_rate(d->format, stream, d->frame);
    AVRational aspect = d->frame->sample_aspect_ratio;
    if (!aspect.num || !aspect.den)
        aspect = (AVRational){1, 1};
    if (!rate.num || !rate.den)
        rate = (AVRational){30000, 1001};
    char args[320];
    snprintf(
        args, sizeof args,
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d:colorspace=%d:range=%d",
        d->frame->width, d->frame->height, d->frame->format, stream->time_base.num,
        stream->time_base.den, aspect.num, aspect.den, rate.num, rate.den, d->frame->colorspace,
        d->frame->color_range);
    d->filters = avfilter_graph_alloc();
    if (!d->filters)
        return AVERROR(ENOMEM);
    d->filters->nb_threads = 1;
    int result = avfilter_graph_create_filter(&d->source, avfilter_get_by_name("buffer"), "source",
                                              args, NULL, d->filters);
    if (result >= 0)
        result = avfilter_graph_create_filter(&d->sink, avfilter_get_by_name("buffersink"), "sink",
                                              NULL, NULL, d->filters);
    if (result < 0)
        return result;
    AVFilterInOut *input = avfilter_inout_alloc(), *output = avfilter_inout_alloc();
    if (!input || !output) {
        avfilter_inout_free(&input);
        avfilter_inout_free(&output);
        return AVERROR(ENOMEM);
    }
    output->name = av_strdup("in");
    output->filter_ctx = d->source;
    input->name = av_strdup("out");
    input->filter_ctx = d->sink;
    if (!input->name || !output->name) {
        avfilter_inout_free(&input);
        avfilter_inout_free(&output);
        return AVERROR(ENOMEM);
    }
    /* Same image transforms as movie.c's desktop path. GT4 stores the two fields
       stacked; il restores alternating lines before yadif emits both fields.
       Full-range scaling retains the disc's grey blacks instead of expanding.
       Run fps before stateless scale/format: FFmpeg 7.1 scale framesync forwards
       its final input PTS as EOF, dropping a final picture if fps follows it. */
    const char *filters =
        d->ipu ? "scale=640:448:in_range=full:out_range=full,format=rgba"
        : d->interlace
            ? "il=l=i:c=i,setfield=tff,yadif=mode=send_field:parity=tff,fps=60000/1001,scale=640:448:in_range=full:out_range=full,format=rgba"
            : "fps=30000/1001,scale=640:448:in_range=full:out_range=full,format=rgba";
    result = avfilter_graph_parse_ptr(d->filters, filters, &input, &output, NULL);
    avfilter_inout_free(&input);
    avfilter_inout_free(&output);
    return result < 0 ? result : avfilter_graph_config(d->filters, NULL);
}
static int rewind_movie(gt4_movie_decoder *d)
{
    int result = av_seek_frame(d->format, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (result < 0)
        return result;
    avcodec_flush_buffers(d->codec);
    avfilter_graph_free(&d->filters);
    d->source = d->sink = NULL;
    d->input_eof = d->filter_eof = 0;
    d->decoded = 0;
    return 0;
}
int gt4_movie_decoder_read(gt4_movie_decoder *d, uint8_t *rgba)
{
    if (!d || !rgba)
        return AVERROR(EINVAL);
    for (;;) {
        int result = d->filters ? av_buffersink_get_frame(d->sink, d->output) : AVERROR(EAGAIN);
        if (result >= 0) {
            for (int y = 0; y < GT4_MOVIE_HEIGHT; y++)
                memcpy(rgba + (size_t)y * GT4_MOVIE_WIDTH * 4,
                       d->output->data[0] + y * d->output->linesize[0], GT4_MOVIE_WIDTH * 4);
            av_frame_unref(d->output);
            return 1;
        }
        if (result != AVERROR(EAGAIN) && result != AVERROR_EOF)
            return result;
        if (d->filter_eof) {
            if (!d->repeat)
                return 0;
            result = rewind_movie(d);
            if (result < 0)
                return result;
        }
        result = avcodec_receive_frame(d->codec, d->frame);
        if (result >= 0) {
            if (!d->filters && (result = make_filters(d)) < 0)
                return result;
            d->frame->pts = d->frame->best_effort_timestamp;
            if (d->frame->pts == AV_NOPTS_VALUE)
                d->frame->pts = av_rescale_q(d->decoded, (AVRational){1001, 30000},
                                             d->format->streams[d->video]->time_base);
            /* MPEG-PS packets can omit duration. buffersrc closes at
               last_pts=pts+duration; zero would discard the final picture. */
            if (d->frame->duration <= 0) {
                AVStream *stream = d->format->streams[d->video];
                AVRational rate = av_guess_frame_rate(d->format, stream, d->frame);
                if (!rate.num || !rate.den)
                    rate = (AVRational){30000, 1001};
                d->frame->duration = av_rescale_q(1, av_inv_q(rate), stream->time_base);
            }
            d->decoded++;
            result = av_buffersrc_add_frame_flags(d->source, d->frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            av_frame_unref(d->frame);
            if (result < 0)
                return result;
            continue;
        }
        if (result == AVERROR_EOF) {
            if (!d->filters)
                return 0;
            result = av_buffersrc_add_frame_flags(d->source, NULL, 0);
            if (result < 0)
                return result;
            d->filter_eof = 1;
            continue;
        }
        if (result != AVERROR(EAGAIN) || d->input_eof)
            return result;
        while ((result = av_read_frame(d->format, d->packet)) >= 0) {
            if (d->packet->stream_index == d->video) {
                result = avcodec_send_packet(d->codec, d->packet);
                av_packet_unref(d->packet);
                break;
            }
            av_packet_unref(d->packet);
        }
        if (result == AVERROR_EOF) {
            d->input_eof = 1;
            result = avcodec_send_packet(d->codec, NULL);
        }
        if (result < 0)
            return result;
    }
}
