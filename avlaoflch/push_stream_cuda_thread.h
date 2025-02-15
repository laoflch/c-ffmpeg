
/*
*/

#ifndef PUSH_STREAM_CUDA_LAOFLCH_THREAD_H
#define PUSH_STREAM_CUDA_LAOFLCH_THREAD_H

#include "complex_filter.h"
#include "drawutils.h"
#include "subtitle.h"
#include <libavutil/threadmessage.h>

typedef struct HandleVideoThreadArg {
    //int (*handle_video_complex_filter)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **,int *,AVFilterGraph **,AVFilterContext **,AVFilterContext **,AVFilterContext **,AVFilterContext **,FilterGraph *,OutputStream **output_streams );

    AVPacket *pkt;
    AVPacket *out_pkt;
    AVFrame *frame;
    AVCodecContext *dec_ctx;
    AVCodecContext **enc_ctx;
    AVFormatContext *fmt_ctx;
    AVFormatContext *ofmt_ctx;
    int out_stream_index;
    int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **);
    InputStream **input_streams;
    int *stream_mapping;
    AVFilterGraph **filter_graph;
    AVFilterContext **mainsrc_ctx;
    AVFilterContext **logo_ctx;
    AVFilterContext **subtitle_ctx;
    AVFilterContext **resultsink_ctx;
    FilterGraph *filter_graph_des;
    OutputStream **output_streams;


    //AVPacket **pkt_point;



    AVThreadMessageQueue **video_pkt_queue;
    //AVThreadMessageQueue **audio_pkt_queue;
    

} HandleVideoThreadArg;

typedef struct HandleAudioThreadArg {
    //int (*handle_video_complex_filter)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **,int *,AVFilterGraph **,AVFilterContext **,AVFilterContext **,AVFilterContext **,AVFilterContext **,FilterGraph *,OutputStream **output_streams );

    AVPacket *pkt;
    AVPacket *out_pkt;
    AVFrame *frame;
    AVCodecContext *dec_ctx;
    AVCodecContext **enc_ctx;
    AVFormatContext *fmt_ctx;
    AVFormatContext *ofmt_ctx;
    int out_stream_index;
    int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **);
    InputStream **input_streams;
    int *stream_mapping;
    AVFilterGraph **filter_graph;
    AVFilterContext **buffersink_ctx;
    AVFilterContext **buffersrc_ctx;
    OutputStream **output_streams;
    char *afilter_desrc;

    //AVPacket **pkt_point;



    //AVThreadMessageQueue **video_pkt_queue;
    AVThreadMessageQueue **audio_pkt_queue;
    

} HandleAudioThreadArg;


typedef struct QueueMsg {
  AVPacket *pkt;
  int magic;

} QueueMsg;


#define MAGIC 0xdeabc0de

//extern static void *grow_array;

/*  push2trtsp_sub_logo_cuda
 * 将视频流通过rtsp协议推送到直播服务器，硬编字幕,并实现logo 的周期淡入淡出
 * 
 *
 * video_file_path:
 * video_index:
 * audio_index:
 * subtitle_file_path:
 * logo_frame:
 * rtsp_pusth_path:
 * if_logo_fade:
 * duration_frames:
 * interval_frames:
 * present_frames:
 * task_handle_process_info:
 *
 **/

int push2rtsp_sub_logo_cuda_thread(const char *video_file_path, const int video_index, const int audio_index,const int subtitle_index,const char *subtitle_file_path,AVFrame **logo_frame,const char *rtsp_push_path,bool if_hw,bool if_logo_fade,uint64_t duration_frames,uint64_t interval_frames,uint64_t present_frames,TaskHandleProcessInfo *task_handle_process_info);

int all_subtitle_logo_native_cuda_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **input_streams,int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **subtitle_ctx,AVFilterContext **resultsink_ctx,FilterGraph *filter_graph_des,OutputStream **output_streams );

//int all_subtitle_logo_native_cuda_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **input_streams,int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **resultsink_ctx,FilterGraph *filter_graph_des,OutputStream **output_streams );


int handle_subtitle2(AVFrame **frame,int64_t pts,AssContext *ass,AVRational time_base);
//int handle_sub(AVFrame *frame,AssContext *ass,AVRational time_base);

#endif /*PUSH_STREAM_CUDA_LAOFLCH_THREAD_H*/
