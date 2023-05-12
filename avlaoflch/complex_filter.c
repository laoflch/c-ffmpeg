/*
 * Copyright (c) 2013 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat/libavcodec demuxing and muxing API example.
 *
 * Remux streams from one container format to another.
 * @example remuxing.c
 */

#include <libavutil/timestamp.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/bprint.h>
#include <libavutil/time.h>//必须引入，否则返回值int64_t无效，默认返回int,则会出现负值情况
#include <libavutil/audio_fifo.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
//#include <libavfilter/drawutils.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/parseutils.h>
#include "complex_filter.h"
//#define have_clock_gettime 1
//#include <sys/time.h> //需要引入sys/time.h的库文件，不然av_gettime_relative返回的时间长度大于int64_t，将返回付值,相减会出现混乱
//#include "ffmpeg.h"
//
//
#define R 0
#define G 1
#define B 2
#define A 3

const static enum AVPixelFormat studio_level_pix_fmts[] = {
    AV_PIX_FMT_YUV444P,  AV_PIX_FMT_YUV422P,  AV_PIX_FMT_YUV420P,
    AV_PIX_FMT_YUV411P,  AV_PIX_FMT_YUV410P,
    AV_PIX_FMT_YUV440P,
    AV_PIX_FMT_NONE
};

static enum AVPixelFormat hw_pix_fmt;

InputStream **input_streams = NULL;
int        nb_input_streams = 0;
OutputStream **output_streams = NULL;
int        nb_output_streams = 0;

static int decode_interrupt_cb(void *ctx)
{

  printf("intrrupt for input steam");
    return 0;

}
const AVIOInterruptCB int_cb = { decode_interrupt_cb, NULL };

//extern ff_vf_fade2;


int64_t av_gettime_relative_customer(void)
{
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        printf("mon:%d sec:%d nsec:%d",CLOCK_MONOTONIC,ts.tv_sec,ts.tv_nsec);
        return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts){
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}
void *grow_array(void *array, int elem_size, int *size, int new_size)
{
    if (new_size >= INT_MAX / elem_size) {
        av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
        return array;
    }
    if (*size < new_size) {
        uint8_t *tmp = av_realloc_array(array, new_size, elem_size);
        if (!tmp)
            return array;
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        return tmp;
    }
    return array;
}


static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

 //printf("#######224222##########");
        printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s time_base:{%d %d} stream_index:%d\n",
           tag,
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           time_base->den,time_base->num,
           pkt->stream_index);


}

static void add_input_streams( AVFormatContext *ic, int stream_index,int input_stream_index,bool hw,enum AVHWDeviceType type,AVBufferRef *hw_device_ctx)
{
    int  ret;

    //for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[stream_index];
        AVCodecParameters *par = st->codecpar;
        InputStream *ist = av_mallocz(sizeof(*ist));
        char *framerate = NULL, *hwaccel_device = NULL;
        const char *hwaccel = NULL;
        char *hwaccel_output_format = NULL;
        char *codec_tag = NULL;
        char *next;
        char *discard_str = NULL;
        const AVClass *cc = avcodec_get_class();
        const AVOption *discard_opt = av_opt_find(&cc, "skip_frame", NULL, 0, 0);


        if (!ist)
            return;

                //input_streams[nb_input_streams - 1] = ist;
        input_streams[input_stream_index] = ist;

        ist->st = st;
     //   ist->file_index = nb_input_files;
        ist->discard = 1;
       // st->discard  = AVDISCARD_ALL;
        ist->nb_samples = 0;
        ist->min_pts = INT64_MAX;
        ist->max_pts = INT64_MIN;
        //if(rate_emu){
        ist->start=av_gettime_relative();
        //}
        ist->ts_scale = 1.0;
        //AVCodecParameters *in_codecpar = ifmt_ctx->streams[0]->codecpar;
//AVRational time_base= st->time_base;
//

        ist->dec = avcodec_find_decoder(par->codec_id);
        if (!ist->dec) {
            fprintf(stderr, "Could not allocate video decodec \n");
            return ; 
        }

//硬解码
        if(hw&&par->codec_type==AVMEDIA_TYPE_VIDEO){
           for (int i = 0;; i++) {
               const AVCodecHWConfig *config = avcodec_get_hw_config(ist->dec, i);
               if (!config) {
                   fprintf(stderr, "Decoder %s does not support device type %s.\n",
                    ist->dec->name, av_hwdevice_get_type_name(type));
            //return -1;
               }
               if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                   config->device_type == type) {
                   hw_pix_fmt = config->pix_fmt;
                   break;
               }
           }
        }
        ist->dec_ctx = avcodec_alloc_context3(ist->dec);
        if (!ist->dec_ctx) {
            fprintf(stderr, "Could not allocate video decodec context\n");
            return ; 
        }
  //      printf("AVStream time_base:{%d %d} i:%d\n",st->time_base.den,st->time_base.num,i);
        ret = avcodec_parameters_to_context(ist->dec_ctx, par);
        if (ret < 0){
            printf("error code %d \n",ret);
            return ;
        }

        /*ist->dec = avcodec_find_decoder(ist->dec_ctx->codec_id);
        if (!ist->dec) {
        fprintf(stderr, "Could not allocate video decodec \n");
        return ; 
    }*/

        ist->dec_ctx->framerate=av_guess_frame_rate(ic,st,NULL);

        if(hw&&par->codec_type==AVMEDIA_TYPE_VIDEO){ 
            ist->dec_ctx->get_format  = get_hw_format;

            if (hw_decoder_init(ist->dec_ctx, type,hw_device_ctx) < 0)
            return ;
        }

        //printf("decodec_id:%d",ist->dec_ctx->codec_id);
 //printf("AVdecodecctx time_base:{%d %d} \n",ist->dec_ctx->time_base.den,ist->dec_ctx->time_base.num);
        //printf("AVdecodecctx time_base:{%d %d} \n",ist->dec_ctx->time_base.den,ist->dec_ctx->time_base.num);

        if ((ret = avcodec_open2(ist->dec_ctx, ist->dec, NULL)) < 0) {
           printf("open codec faile %d \n",ret);
           return ;
        }

        printf("dec_ctx:%d",ist->dec_ctx->frame_size);

        //
//ist->dec_ctx->time_base=st->time_base;

 //printf("AVdecodecctx time_base:{%d %d} \n",ist->dec_ctx->time_base.den,ist->dec_ctx->time_base.num);
        //MATCH_PER_STREAM_OPT(ts_scale, dbl, ist->ts_scale, ic, st);
       
        //guess_input_channel_layout(ist);

}

static OutputStream *new_output_stream(AVFormatContext *oc,  int i,int codec_type,int codec_id )
{
    OutputStream *ost;
    //AVStream *st = avformat_new_stream(oc, NULL);

    //AVCodecParameters *par = st->codecpar;

    int idx      = oc->nb_streams - 1, ret = 0;
    const char *bsfs = NULL, *time_base = NULL;
    char *next, *codec_tag = NULL;
    double qscale = -1;
    //int i;


    
    //GROW_ARRAY(output_streams, nb_output_streams);
    if (!(ost = av_mallocz(sizeof(*ost))))
        return NULL;

    output_streams[i] = ost;

    //ost->file_index = nb_output_files - 1;
    ost->index      = idx;
    //ost->st         = st;
    ost->forced_kf_ref_pts = AV_NOPTS_VALUE;

    ost->audio_fifo=NULL;

    //if(codec_type== AVMEDIA_TYPE_AUDIO&&if_audio_fifo){
    //ost->audio_fifo=av_mallocz(sizeof(AVAudioFifo *));
    //};
    
    //st->codecpar->codec_type = codec_type;
    //printf("encodec_id:%d",codec_id);
    ost->enc = avcodec_find_encoder(codec_id);
        if (!ost->enc) {
        fprintf(stderr, "Could not allocate video encodec \n");
        return NULL; 
    }

    ost->enc_ctx = avcodec_alloc_context3(ost->enc);
        if (!ost->enc_ctx) {
        fprintf(stderr, "Could not allocate video encodec context\n");
        return NULL; 
    }


    //ret = avcodec_parameters_to_context(ost->enc_ctx, par);
    //if (ret < 0){
     //       printf("error code %d \n",ret);
      //      return NULL;
    //}
    
  

    return ost;
}

int hw_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping);

int simple_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping){

//AVFrame *frame;

  if(pkt!=NULL){

  //  AVFrame *frame;
   // frame = av_frame_alloc();
   // if (!frame) {
    //    fprintf(stderr, "Could not allocate video frame\n");
     //   return 1;
    //}
//必须初始化frame的format,width,height
  //  frame->format = dec_ctx->pix_fmt;
   // frame->width  = dec_ctx->width;
    //frame->height = dec_ctx->height;
    //
    
//AVStream *in_stream  = fmt_ctx->streams[pkt->stream_index];

        //out_pkt->stream_index = out_stream_index;
        //
  //     printf("out_pkt stream_index:%d\n",pkt->stream_index); 
 //       out_pkt->stream_index=pkt->stream_index;
  //      AVStream *out_stream = ofmt_ctx->streams[out_pkt->stream_index];

    

    int ret;
 /*ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        return 1;
   }*/

   // ret=avcodec_receive_frame(dec_ctx,frame);

    //if(ret>=0){
     // if (frame->pts==AV_NOPTS_VALUE){
      //  frame->pts=frame->best_effort_timestamp;

       // printf("set video pts %"PRId64"\n",frame->pts);
      //}

    //}

 //printf("stream time base5-1:{%d %d} i:%d\n",input_streams[pkt->stream_index]->dec_ctx->time_base.den,input_streams[pkt->stream_index]->dec_ctx->time_base.num,pkt->stream_index);
 //
 //dec_ctx->time_base=in_stream->time_base;
 // printf("dec time base:{%d %d}",dec_ctx->time_base.den,dec_ctx->time_base.num);

  printf("dec time base:{%d %d}",dec_ctx->time_base.den,dec_ctx->time_base.num);
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }
//dec_ctx->time_base=in_stream->time_base;
    //av_packet_unref(pkt);
//printf("stream time base5-2:{%d %d} i:%d\n",input_streams[pkt->stream_index]->dec_ctx->time_base.den,input_streams[pkt->stream_index]->dec_ctx->time_base.num,pkt->stream_index);

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
 
      if (ret == AVERROR(EAGAIN)) {
 return ret;

      }
else if( ret == AVERROR_EOF){
                  return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);
            return 1 ;
        }
//log_packet(fmt_ctx, pkt, "out-1");    /* send the frame to the encoder */
    if (frame!=NULL)
        printf("Send frame %"PRId64" size %"PRId64" aspect{%d %d} \n", frame->pts,frame->linesize,frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num);
    
  
    frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
    int ret_enc = avcodec_send_frame(*enc_ctx, frame);
//printf("return code %d",ret_enc);
    if (ret_enc < 0) {
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
        continue;
    }

    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
//printf("%s: pts:%"PRId64,"out_pkt",out_pkt->pts);
//printf("%s: dts:%"PRId64,"out_pkt",out_pkt->dts);
          if (ret_enc == AVERROR(EAGAIN)) {
        //printf("$$$$$$$222222$$$$$$$$$$");
 //printf("pkt_out size : %d \n",out_pkt->size);

break;

      }
else if( ret_enc == AVERROR_EOF){
            break;
      }
        else if (ret_enc < 0) {
            fprintf(stderr, "Error during encoding\n");
            return 1;
        }
if (out_pkt->data!=NULL) {
      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }
    //输出流
    //
    ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);
    if(ret_enc<0){
        printf("interleaved write error:%d",ret_enc);
        return ret_enc;
    }

    }


    
    }
    return 0;
  }else{
      return 1;
  } 

}

int simple_audio_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping, int s_pts){



  //AVFrame *frame;

  if(pkt!=NULL){


    int ret;
  printf("dec time base:{%d %d}",dec_ctx->time_base.den,dec_ctx->time_base.num);

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {

        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }


    while (ret >= 0) {


        ret = avcodec_receive_frame(dec_ctx, frame);
 
      if (ret == AVERROR(EAGAIN)) {
        return ret;
      }else if( ret == AVERROR_EOF){
                 return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);


            return 1 ;
        }
    if (frame!=NULL)
        printf("Send frame %"PRId64" size %"PRId64" aspect{%d %d} \n", frame->pts,frame->linesize,frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num);
    int enc_frame_size=(*enc_ctx)->frame_size;
    if(dec_ctx->frame_size!=enc_frame_size){
              printf("diff frame size\n");

        if ((*enc_ctx)->codec_type==AVMEDIA_TYPE_AUDIO &&
        output_streams[out_stream_index]->audio_fifo!=NULL){
               //uint8_t** new_data=frame->extended_data;
      //int new_size=frame->nb_samples;
       ret=write_frame_to_audio_fifo(output_streams[out_stream_index]->audio_fifo, frame->extended_data,  frame->nb_samples);
      //printf("write audio bytes:%d\n");
      if (ret<0){
            printf("write aframe to fifo error");
            return 1;
      }
      if (!s_pts){
//设置返回pts为输入帧的pts
      s_pts=frame->pts;
           //s_pts=pkt->pts;
      }
     
      } 
      //printf("av_audio_fifo_size:%d\n",av_audio_fifo_size(output_streams[out_stream_index]->audio_fifo));

    while ((av_audio_fifo_size(output_streams[out_stream_index]->audio_fifo))>=enc_frame_size){
       AVFrame *frame_enc=NULL;

      bool flushing=av_audio_fifo_size(output_streams[out_stream_index]->audio_fifo)==0 ;
      if (frame_enc!=NULL){
               }
       if (!flushing)
       {
         ret=read_frame_from_audio_fifo(output_streams[out_stream_index]->audio_fifo,*enc_ctx,&frame_enc);
         if(ret<0){
             printf("read frame from ctx error");
             return 1;
         }
                       } 
    int ret_enc = avcodec_send_frame(*enc_ctx, frame_enc);
    if (ret_enc < 0) {
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
        continue;
    } 
    //printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    //
    //接收编码完成的packet
    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
          if (ret_enc == AVERROR(EAGAIN)) {
              //av_freep(frame->extended_data);
              //av_freep(&(frame->extended_data[0]));
               av_frame_unref(frame_enc);
              av_frame_free(&frame_enc);
              break;
         }else if( ret_enc == AVERROR_EOF){
              av_freep(frame->extended_data);
 av_freep(&(frame->extended_data[0]));
              av_frame_unref(frame_enc);
              av_frame_free(&frame_enc);

            break;

    
      }else if (ret_enc < 0) {
            fprintf(stderr, "Error during encoding\n");
              av_freep(frame->extended_data);

              av_freep(&(frame->extended_data[0]));
 //av_freep(frame_enc->extended_data);
    //av_freep(&(frame_enc->data[0]));
              av_frame_unref(frame_enc);
              av_frame_free(&frame_enc);

            return 1;
        }
              

 if (out_pkt->data!=NULL) {
   printf("s_pts:%d ret:%d\n",s_pts,ret);

          //编码后pkt的pts是空的，需要重新设置
 out_pkt->pts=s_pts;
 out_pkt->dts=s_pts;
 s_pts += ret;



      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }
    //输出流
    //
    ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame_enc,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);

//printf("########77777########");
    //av_freep(frame->extended_data);
    //av_freep(&(frame->data[0]));
    av_freep(frame_enc->extended_data);
    av_freep(&(frame_enc->data[0]));


    //av_frame_unref(rame});

    av_frame_unref(frame_enc);
    av_frame_free(&frame_enc);

    if(ret_enc<0){
        printf("interleaved write error:%d",ret_enc);
        return ret_enc;
    }
    //s_pts++; 
 //printf(" ret:%d\n",ret_enc);

    }
    }
//av_frame_unref(frame);
av_packet_unref(pkt);//确保非输出packet也释放
    return s_pts;  
    }else{

       printf("same frame size\n");
       int ret_enc = avcodec_send_frame(*enc_ctx, frame);
       if (ret_enc < 0) {
          fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
          continue;
       }
       while (ret_enc >= 0) {
          ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
          if (ret_enc == AVERROR(EAGAIN)) {

            break;

          }else if( ret_enc == AVERROR_EOF){
       
             break;

    
          }else if (ret_enc < 0) {
             fprintf(stderr, "Error during encoding\n");
             return 1;
          }
          ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);
          if(ret_enc<0){
             printf("interleaved write error:%d",ret_enc);
             return ret_enc;
          } 
          return frame->pts; 
       }

    //输出流
    //
       
     }
   }
    return 0;
  }else{
      return 1;
  } 


}


int subtitle_logo_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **resultsink_ctx,FilterGraph *filter_graph_des ){
    if(pkt!=NULL){
 AVFrame *sw_frame =NULL; //;

    int ret,s_pts;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
      if (ret == AVERROR(EAGAIN)) {

        av_frame_unref(frame);
//printf("----------------------------------");
 return ret;

      }
else if( ret == AVERROR_EOF){
                  return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);
            return 1 ;
        }
     if (filter_graph_des->if_hw &&frame->format == hw_pix_fmt) {
          s_pts=frame->pts;

         sw_frame=av_frame_alloc();

                  /* retrieve data from GPU to CPU */
         if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
               // goto fail;
         }
         //sw_frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
         sw_frame->pts=s_pts;

         //av_frame_free(&frame);
         //
         //av_free(frame->data[0]);
         av_frame_unref(frame) ;
        // av_frame_free(&frame);
//memcpy(frame, sw_frame,sizeof(AVFrame));

         //frame = sw_frame;
     }else{
         sw_frame=frame;
     } 

     /*if (sw_frame->pts>0){
       sw_frame->pts=filter_graph_des->current_frame_pts;

       filter_graph_des->current_frame_pts++;

     }
*/
//log_packet(fmt_ctx, pkt, "out-1");    /* send the frame to the encoder */ 

    if (sw_frame!=NULL)
        printf("Send frame %"PRId64" %"PRId64" size %"PRId64" aspect{%d %d} time_base{%d %d}\n", sw_frame->pts,filter_graph_des->current_frame_pts,sw_frame->linesize,sw_frame->sample_aspect_ratio.den,sw_frame->sample_aspect_ratio.num,dec_ctx->time_base.num,dec_ctx->time_base.den);
//printf("##########################331");
   //处理视频滤镜 
    //这两个变量在本文里没有用的，只是要传进去。
                AVFilterInOut *inputs, *cur, *outputs;
                    AVRational logo_tb = {0};
                    AVRational logo_fr = {0};

AVFrame *logo_frame; 

                    if(*(filter_graph_des->logo_frame)==NULL){ 
logo_frame = get_frame_from_jpeg_or_png_file2("/workspace/ffmpeg/FFmpeg/doc/examples/laoflch-mc-log.png",&logo_tb,&logo_fr);

filter_graph_des->logo_frame=&logo_frame;}else{
  logo_frame=*(filter_graph_des->logo_frame);
}
 //printf("logo frame pix_fmt %d \n", logo_frame->format);


                    if(  *filter_graph==NULL ){
AVFilterGraph *filter_graph_point;
AVFilterContext *mainsrc_ctx_point,*logo_ctx_point,*resultsink_ctx_point ;
                    int64_t logo_next_pts = 0,main_next_pts = 0;
                                                            //AVFrame *logo_frame = get_frame_from_jpeg_or_png_file2("/workspace/ffmpeg/FFmpeg/doc/examples/laoflch-mc-log.png",&logo_tb,&logo_fr);
                    //初始化滤镜容器
                    filter_graph_point = avfilter_graph_alloc();

                    *filter_graph=filter_graph_point;
                    if (!filter_graph) {
                        printf("Error: allocate filter graph failed\n");
                        return -1;
                    }

                    // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                    //AVRational tb = fmt_ctx->streams[0]->time_base;
                    //time_base 必须与输入的dec_ctx的time一致，字幕才能正常显示
                    AVRational tb = dec_ctx->time_base;
                    AVRational fr = dec_ctx->framerate;//av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[0], NULL);
                    //AVRational logo_sar = logo_frame->sample_aspect_ratio;
                    AVRational sar = sw_frame->sample_aspect_ratio;
                    AVBPrint args;
                    av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);



                    av_bprintf(&args,
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[main];"
                               "[main]subtitles=%s[video_subtitle];"
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[logo];"//[logo]fade=out:240:480[logo_fade];"
                               
                               "[video_subtitle][logo]overlay=x=%d:y=%d[result];"
                               "[result]format=nv12[result_2];" 
                               "[result_2]buffersink",
                               sw_frame->width, sw_frame->height, sw_frame->format, tb.num,tb.den,sar.num, sar.den,fr.num, fr.den,
                               filter_graph_des->subtitle_path,
                               //logo_frame->width, logo_frame->height, logo_frame->format, logo_tb.num,logo_tb.den ,
                               //logo_sar.num,logo_sar.den, logo_fr.num, logo_fr.den,                       
                               450,85,26,1001,48000,2835,2835,24000,1001,
                               //sw_frame->width-logo_frame->width,sw_frame->height-logo_frame->height);
                               sw_frame->width-450,sw_frame->height-85);
                                                    ret = avfilter_graph_parse2(filter_graph_point, args.str, &inputs, &outputs);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }


                    //正式打开滤镜
                    ret = avfilter_graph_config(filter_graph_point, NULL);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }

 //printf("**********************************88");


                    //根据 名字 找到 AVFilterContext
                    mainsrc_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");
*mainsrc_ctx=mainsrc_ctx_point;
                    logo_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_2");
                    *logo_ctx=logo_ctx_point;
                    resultsink_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffersink_5");
                    *resultsink_ctx=resultsink_ctx_point;
                    //ret = av_buffersrc_add_frame_flags(logo_ctx_point, logo_frame,AV_BUFFERSRC_FLAG_PUSH);
                    //if(ret < 0){
                     //   printf("Error: av_buffersrc_add_frame failed\n");
                      //  return ret;
                    //}
                    //因为 logo 只有一帧，发完，就需要关闭 buffer 滤镜
                    //logo_next_pts = logo_frame->pts + logo_frame->pkt_duration;
                    //ret = av_buffersrc_close(logo_ctx_point, logo_next_pts, AV_BUFFERSRC_FLAG_PUSH);

                    /* 发 null 也可以，但是不太正规。可能某些场景下会有问题
                    ret = av_buffersrc_add_frame(logo_ctx, NULL);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }*/

                }
//printf("Logo frame w:%d h:%d pix_fmt:%d aspect{%d %d} \n",logo_frame->width,logo_frame->height,logo_frame->format, logo_frame->sample_aspect_ratio.den,logo_frame->sample_aspect_ratio.num);
                     //else
   //ret = av_buffersrc_add_frame_flags(logo_ctx_point, logo_frame,AV_BUFFERSRC_FLAG_PUSH);
                    //if(ret < 0){
                     //   printf("Error: av_buffersrc_add_frame failed\n");
                      //  return ret;
                    //}
                                //if( frame_num <= 10 ){
                                //
                                //
                                //
             /**
              * 根据overlay的叠加顺序，必须上层先追加frame，再追加下层,如:logo_ctx必须先add，之后才能add mainsrc_ctx
              * 如果帧要重复使用，则add的必须选用AV_BUFFERSRC_FLAG_KEEP_REF这个选项，确保frame被复制之后再add，且add之后不清空frame数据
              * 如果图片要转换成视频流，则必须设置frame的pts
              *
              *
              */                   
                     logo_frame->pts=sw_frame->pts;

                     int frame_size=av_image_get_buffer_size(logo_frame->format,logo_frame->width,logo_frame->height,1);
                     
                     uint8_t* tmp_buffer=(uint8_t*)av_malloc(frame_size*sizeof(uint8_t)); 

                AVFrame *new_logo_frame=av_frame_alloc();

                new_logo_frame->width=logo_frame->width;
                new_logo_frame->height=logo_frame->height;
                new_logo_frame->format=logo_frame->format;

                ret=av_image_fill_arrays(new_logo_frame->data,new_logo_frame->linesize,tmp_buffer,logo_frame->format,new_logo_frame->width,new_logo_frame->height,1);
//printf(" fill %d",ret);
                av_frame_copy(new_logo_frame, logo_frame);
                new_logo_frame->pts=logo_frame->pts;
                     if (filter_graph_des->if_fade ){

                     handle_logo_fade(new_logo_frame,filter_graph_des->duration_frames,filter_graph_des->interval_frames,filter_graph_des->present_frames);


                     }
ret = av_buffersrc_add_frame_flags(*logo_ctx, new_logo_frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }

                   //av_free(new_logo_frame->data[0]); 
                    av_frame_unref(new_logo_frame);

                    av_frame_free(&new_logo_frame);
                    av_freep(&tmp_buffer);
            
                    ret = av_buffersrc_add_frame_flags(*mainsrc_ctx, sw_frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){

 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);

                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }

                    //logo_frame->pts=sw_frame->pts;
                //av_free(frame->data[0]);
                //av_frame_unref(frame);
                //}
                ret = av_buffersink_get_frame_flags(*resultsink_ctx, frame,AV_BUFFERSINK_FLAG_NO_REQUEST);
                if( ret == AVERROR_EOF ){
                    //没有更多的 AVFrame
                    printf("no more avframe output \n");
                }else if( ret == AVERROR(EAGAIN) ){


                    //需要输入更多的 AVFrame
                    printf("need more avframe input \n");
                }


                //滤镜处理结束

printf("after filter frame w:%d h:%d pix_fmt:%d aspect{%d %d} \n",frame->width,frame->height,frame->format, frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num);
  
    frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
    int ret_enc = avcodec_send_frame(*enc_ctx, frame);
//printf("return code %d",ret_enc);
    if (ret_enc < 0) {
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
        continue;
    }

    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
//printf("%s: pts:%"PRId64,"out_pkt",out_pkt->pts);
//printf("%s: dts:%"PRId64,"out_pkt",out_pkt->dts);
          if (ret_enc == AVERROR(EAGAIN)) {
        //printf("$$$$$$$222222$$$$$$$$$$");
 //printf("pkt_out size : %d \n",out_pkt->size);
 //
   av_free(sw_frame->data[0]);
 av_frame_unref(frame);
 av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
break;

      }
else if( ret_enc == AVERROR_EOF){
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
            break;
      }
        else if (ret_enc < 0) {
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
            fprintf(stderr, "Error during encoding\n");
           break; 
        }
if (out_pkt->data!=NULL) {
      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }
    //输出流
    //
    ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);
 //if(sw_frame!=NULL){
    //av_frame_unref(sw_frame);
    //av_frame_free(&sw_frame);
    //sw_frame=NULL;
 //}
 if(ret_enc<0){
        printf("interleaved write error:%d",ret_enc);
        return ret_enc;
    }
 //av_frame_unref(sw_frame);
    //av_frame_free(&sw_frame);
    //sw_frame=NULL;
    }

 //av_frame_unref(sw_frame);
    //av_frame_free(&sw_frame);
   // sw_frame=NULL;

    
    }
    av_frame_unref(frame);
    av_free(sw_frame->data[0]);
    av_frame_unref(sw_frame);
    av_frame_free(&sw_frame);
    
    sw_frame=NULL;

    return 0;
  }else{
      return 1;
  } 

}

int subtitle_logo_video_codec_func2(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **resultsink_ctx,FilterGraph *filter_graph_des ){
    if(pkt!=NULL){
 AVFrame *sw_frame =NULL; //;

    int ret,s_pts;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
      if (ret == AVERROR(EAGAIN)) {

        av_frame_unref(frame);
//printf("----------------------------------");
 return ret;

      }
else if( ret == AVERROR_EOF){
                  return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);
            return 1 ;
        }
     if (filter_graph_des->if_hw &&frame->format == hw_pix_fmt) {
          s_pts=frame->pts;

         sw_frame=av_frame_alloc();

                  /* retrieve data from GPU to CPU */
         if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
               // goto fail;
         }
         //sw_frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
         sw_frame->pts=s_pts;

         //av_frame_free(&frame);
         av_frame_unref(frame) ;
        // av_frame_free(&frame);
//memcpy(frame, sw_frame,sizeof(AVFrame));

         //frame = sw_frame;
     }else{
         sw_frame=frame;
     } 

//log_packet(fmt_ctx, pkt, "out-1");    /* send the frame to the encoder */
    //if (frame!=NULL)
        //printf("Send frame %"PRId64" size %"PRId64" aspect{%d %d} \n", frame->pts,frame->linesize,frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num);
   //处理视频滤镜 
    //这两个变量在本文里没有用的，只是要传进去。
                AVFilterInOut *inputs, *cur, *outputs;


                    if(  *filter_graph==NULL ){
AVFilterGraph *filter_graph_point;
AVFilterContext *mainsrc_ctx_point,*logo_ctx_point,*resultsink_ctx_point ;
                    int64_t logo_next_pts = 0,main_next_pts = 0;
                    AVRational logo_tb = {0};
                    AVRational logo_fr = {0};
                                        AVFrame *logo_frame = get_frame_from_jpeg_or_png_file2("/workspace/ffmpeg/FFmpeg/doc/examples/laoflch-mc-log.png",&logo_tb,&logo_fr);
                    avfilter_register_all();
                    //avfilter_register(ff_vf_fade2);

                    //初始化滤镜容器
                    filter_graph_point = avfilter_graph_alloc();
                                       *filter_graph=filter_graph_point;
                    if (!filter_graph) {
                        printf("Error: allocate filter graph failed\n");
                        return -1;
                    }

                    // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                    //AVRational tb = fmt_ctx->streams[0]->time_base;
                    //time_base 必须与输入的dec_ctx的time一致，字幕才能正常显示
                    AVRational tb = dec_ctx->time_base;
                    AVRational fr = dec_ctx->framerate;//av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[0], NULL);
                    AVRational logo_sar = logo_frame->sample_aspect_ratio;
                    AVRational sar = sw_frame->sample_aspect_ratio;
                    AVBPrint args_main,args_logo,args_overlay;
                    av_bprint_init(&args_main, 0, AV_BPRINT_SIZE_AUTOMATIC);
                    av_bprintf(&args_main,
                               "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d", sw_frame->width, sw_frame->height, sw_frame->format, tb.num,tb.den,sar.num, sar.den,fr.num, fr.den);
                   
                    //根据 名字 找到 AVFilterContext
                    AVFilter* buffer_main=avfilter_get_by_name("buffer");

                    ret=avfilter_graph_create_filter(mainsrc_ctx, buffer_main, "main",args_main.str, NULL, filter_graph_point);
                    
                    if (ret < 0) {
                        printf("Cannot create graph filter %s\n","main");
                        return ret;
                    }
                    
                    AVBPrint args_subtitle;
                    av_bprint_init(&args_subtitle, 0, AV_BPRINT_SIZE_AUTOMATIC);

                    av_bprintf(&args_subtitle,
                               "%s", filter_graph_des->subtitle_path);
                    AVFilter* buffer_subtitle=avfilter_get_by_name("subtitles");
                    AVFilterContext **subtitle_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));

                    ret=avfilter_graph_create_filter(subtitle_ctx, buffer_subtitle, "subtitle",args_subtitle.str, NULL, filter_graph_point);
                    
                    if (ret < 0) {
                        printf("Cannot create graph filter %s\n","subtitle");
                        return ret;
                    }

                    //mainsrc_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");
                    //*mainsrc_ctx=mainsrc_ctx_point;
                    av_bprint_init(&args_logo, 0, AV_BPRINT_SIZE_AUTOMATIC);

                    av_bprintf(&args_logo,
                               "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d", logo_frame->width, logo_frame->height, logo_frame->format, logo_tb.num,logo_tb.den,logo_sar.num,logo_sar.den, logo_fr.num, logo_fr.den);
                    AVFilter* buffer_logo=avfilter_get_by_name("buffer");

                    ret=avfilter_graph_create_filter(logo_ctx, buffer_logo, "logo",args_logo.str, NULL, filter_graph_point);
                    
                    if (ret < 0) {
                        printf("Cannot create graph filter %s\n","logo");
                        return ret;
                    }
                    AVBPrint args_fade;
                    av_bprint_init(&args_fade, 0, AV_BPRINT_SIZE_AUTOMATIC);

                    av_bprintf(&args_fade,
                               "t=in:st=%d:d=%d:alpha=1", 5,120);
                    AVFilter* buffer_fade=avfilter_get_by_name("fade2");
                    AVFilterContext **fade2_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));

                    ret=avfilter_graph_create_filter(fade2_ctx, buffer_fade, "logo_fade",args_fade.str, NULL, filter_graph_point);
                    
                    if (ret < 0) {
                        printf("Cannot create graph filter %s\n","fade2");
                        return ret;
                    }
                    
                    av_bprint_init(&args_overlay, 0, AV_BPRINT_SIZE_AUTOMATIC);

                    av_bprintf(&args_overlay,
                               "x=%d:y=%d", sw_frame->width-logo_frame->width,sw_frame->height-logo_frame->height);
                    AVFilter* buffer_overlay=avfilter_get_by_name("overlay");
                    AVFilterContext **overlay_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));

                    ret=avfilter_graph_create_filter(overlay_ctx, buffer_overlay, "overlay",args_overlay.str, NULL, filter_graph_point);
                    
                    if (ret < 0) {
                        printf("Cannot create graph filter %s\n","overlay");
                        return ret;
                    }

                    AVBufferSinkParams *buffers_sink_params=av_buffersink_params_alloc();


                    AVFilter* buffer_sink=avfilter_get_by_name("buffersink");
                    if(filter_graph_des->if_hw){
                        enum AVPixelFormat pix_fmts[]={AV_PIX_FMT_NV12,AV_PIX_FMT_NONE};
                        buffers_sink_params->pixel_fmts=pix_fmts;

                    }else{
                        enum AVPixelFormat pix_fmts[]={AV_PIX_FMT_YUV420P,AV_PIX_FMT_NONE};
                        buffers_sink_params->pixel_fmts=pix_fmts;

                    }
                    ret=avfilter_graph_create_filter(resultsink_ctx, buffer_sink, "result",NULL,NULL,filter_graph_point);
                    
                    if (ret < 0) {
                        printf("Cannot create graph filter %s\n","result");
                        return ret;
                    }

                    /**
                     *滤镜链图
                     *
                     *
                     *           [main]                  [result]
                     * buffer -----------> subtitle ------------> overlay ------> buffersink
                     *                                    ^
                     *                                    | 
                     *                  [logo]            |
                     * buffer ----------------------------+
                     *
                     *
                     *
                     *
                     */

                    ret =avfilter_link(*mainsrc_ctx, 0, *subtitle_ctx, 0);
                    if (ret < 0) {
                        printf("Cannot link filter %s to %s \n","main","subtitle");
                        return ret;
                    }
                    ret =avfilter_link(*subtitle_ctx, 0, *overlay_ctx, 0);
                    if (ret < 0) {
                        printf("Cannot link filter %s to %s \n","subtitle","overlay");
                        return ret;
                    }
                    //ret =avfilter_link(*logo_ctx, 0, *fade_ctx, 0);
                    //if (ret < 0) {
                    //    printf("Cannot link filter %s to %s \n","logo","fade");
                    //    return ret;
                    //}
                    ret =avfilter_link(*logo_ctx, 0, *overlay_ctx, 1);
                    if (ret < 0) {
                        printf("Cannot link filter %s to %s \n","fade","overlay");
                        return ret;
                    }
                    ret =avfilter_link(*overlay_ctx, 0, *resultsink_ctx, 0);
                    if (ret < 0) {
                        printf("Cannot link filter %s to %s \n","overlay","result");
                        return ret;
                    }
                    ret = avfilter_graph_config(*filter_graph, NULL);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }
                    ret = av_buffersrc_add_frame_flags(*logo_ctx, logo_frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }
                    logo_next_pts = logo_frame->pts + logo_frame->pkt_duration;
                    ret = av_buffersrc_close(*logo_ctx, logo_next_pts, AV_BUFFERSRC_FLAG_PUSH);

                    /*ret = avfilter_graph_parse2(filter_graph_point, args.str, &inputs, &outputs);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }
                    //正式打开滤镜
                    ret = avfilter_graph_config(filter_graph_point, NULL);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }
                    //根据 名字 找到 AVFilterContext
                    mainsrc_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");
*mainsrc_ctx=mainsrc_ctx_point;
                    logo_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_2");
                    *logo_ctx=logo_ctx_point;
                    resultsink_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffersink_5");
                    *resultsink_ctx=resultsink_ctx_point;
                    ret = av_buffersrc_add_frame_flags(logo_ctx_point, logo_frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }
                    */
                    //因为 logo 只有一帧，发完，就需要关闭 buffer 滤镜
                   
                    /* 发 null 也可以，但是不太正规。可能某些场景下会有问题
                    ret = av_buffersrc_add_frame(logo_ctx, NULL);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }*/

                }
 //else

                   //if( frame_num <= 10 ){
                    ret = av_buffersrc_add_frame_flags(*mainsrc_ctx, sw_frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){

 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
                        printf("Error: av_buffersrc_add_frame failed\n");
                        //return ret;
                    }
                //}
                ret = av_buffersink_get_frame_flags(*resultsink_ctx, frame,AV_BUFFERSINK_FLAG_NO_REQUEST);
                if( ret == AVERROR_EOF ){
                    //没有更多的 AVFrame
                    printf("no more avframe output \n");
                }else if( ret == AVERROR(EAGAIN) ){


                    //需要输入更多的 AVFrame
                    printf("need more avframe input \n");
                }


                //滤镜处理结束


  
    frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
    int ret_enc = avcodec_send_frame(*enc_ctx, frame);
//printf("return code %d",ret_enc);
    if (ret_enc < 0) {
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
        continue;
    }

    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
//printf("%s: pts:%"PRId64,"out_pkt",out_pkt->pts);
//printf("%s: dts:%"PRId64,"out_pkt",out_pkt->dts);
          if (ret_enc == AVERROR(EAGAIN)) {
        //printf("$$$$$$$222222$$$$$$$$$$");
 //printf("pkt_out size : %d \n",out_pkt->size);
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
 //}
break;

      }
else if( ret_enc == AVERROR_EOF){
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
            break;
      }
        else if (ret_enc < 0) {
 av_frame_unref(frame);
   av_frame_unref(sw_frame);
 //if(sw_frame!=NULL){
 av_frame_free(&sw_frame);
            fprintf(stderr, "Error during encoding\n");
           break; 
        }
if (out_pkt->data!=NULL) {
      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }
    //输出流
    //
    ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);
 //if(sw_frame!=NULL){
    av_frame_unref(sw_frame);
    av_frame_free(&sw_frame);
    sw_frame=NULL;
 //}
 if(ret_enc<0){
        printf("interleaved write error:%d",ret_enc);
        return ret_enc;
    }
 av_frame_unref(sw_frame);
    av_frame_free(&sw_frame);
    sw_frame=NULL;
    }

 av_frame_unref(sw_frame);
    av_frame_free(&sw_frame);
    sw_frame=NULL;

    
    }
    av_frame_unref(sw_frame);
    av_frame_free(&sw_frame);
    sw_frame=NULL;

    return 0;
  }else{
      return 1;
  } 

}

int complex_filter_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **resultsink_ctx, FilterGraph *filter_graph_des){

//AVFrame *frame;

  if(pkt!=NULL){
    int ret;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
      if (ret == AVERROR(EAGAIN)) {
//printf("----------------------------------");
 return ret;

      }
else if( ret == AVERROR_EOF){
                  return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);
            return 1 ;
        }

//log_packet(fmt_ctx, pkt, "out-1");    /* send the frame to the encoder */
    //if (frame!=NULL)
        //printf("Send frame %"PRId64" size %"PRId64" aspect{%d %d} \n", frame->pts,frame->linesize,frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num);
   //处理视频滤镜 
    //这两个变量在本文里没有用的，只是要传进去。
                AVFilterInOut *inputs, *cur, *outputs;


                    if(  *filter_graph==NULL ){
 //printf("****************************");

AVFilterGraph *filter_graph_point;
AVFilterContext *mainsrc_ctx_point,*logo_ctx_point,*resultsink_ctx_point ;
                    int64_t logo_next_pts = 0,main_next_pts = 0;
                    AVRational logo_tb = {0};
                    AVRational logo_fr = {0};
                                        AVFrame *logo_frame = get_frame_from_jpeg_or_png_file2("./laoflch-mc-log.png",&logo_tb,&logo_fr);
                    //初始化滤镜容器
                    filter_graph_point = avfilter_graph_alloc();

                    *filter_graph=filter_graph_point;
                    if (!filter_graph) {
                        printf("Error: allocate filter graph failed\n");
                        return -1;
                    }

                    // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                    //AVRational tb = fmt_ctx->streams[0]->time_base;
                    //time_base 必须与输入的dec_ctx的time一致，字幕才能正常显示
                    AVRational tb = dec_ctx->time_base;
                    AVRational fr = dec_ctx->framerate;//av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[0], NULL);
                    AVRational logo_sar = logo_frame->sample_aspect_ratio;
                    AVRational sar = frame->sample_aspect_ratio;
                    AVBPrint args;
                    av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                    av_bprintf(&args,
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[main];"
                               "[main]subtitles=/workspace/ffmpeg/FFmpeg/doc/examples/Se7en.1995.REMASTERED.1080p.BluRay.x264-SADPANDA.zh.ass[video_subtitle];"
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[logo];"
                               "[video_subtitle][logo]overlay=x=%d:y=%d[result];"
                               "[result]format=yuv420p[result_2];" 
                               //"[result_2]subtitles=/workspace/ffmpeg/FFmpeg/doc/examples/Se7en.1995.REMASTERED.1080p.BluRay.x264-SADPANDA.zh.ass[result_3];"
                               "[result_2]buffersink",
                               frame->width, frame->height, frame->format, tb.num,tb.den,sar.num, sar.den,fr.num, fr.den,
                               logo_frame->width, logo_frame->height, logo_frame->format, logo_tb.num,logo_tb.den ,
                               logo_sar.num,logo_sar.den, logo_fr.num, logo_fr.den,frame->width-logo_frame->width,frame->height-logo_frame->height);
                    ret = avfilter_graph_parse2(filter_graph_point, args.str, &inputs, &outputs);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }
                    //正式打开滤镜
                    ret = avfilter_graph_config(filter_graph_point, NULL);
                    if (ret < 0) {
                        printf("Cannot configure graph\n");
                        return ret;
                    }
                    //根据 名字 找到 AVFilterContext
                    mainsrc_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");
*mainsrc_ctx=mainsrc_ctx_point;
                    logo_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_2");
                    
                    *logo_ctx=logo_ctx_point;
                    resultsink_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffersink_5");
                    *resultsink_ctx=resultsink_ctx_point;
                    ret = av_buffersrc_add_frame_flags(logo_ctx_point, logo_frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }
                    //因为 logo 只有一帧，发完，就需要关闭 buffer 滤镜
                    logo_next_pts = logo_frame->pts + logo_frame->pkt_duration;
                    ret = av_buffersrc_close(logo_ctx_point, logo_next_pts, AV_BUFFERSRC_FLAG_PUSH);

                    /* 发 null 也可以，但是不太正规。可能某些场景下会有问题
                    ret = av_buffersrc_add_frame(logo_ctx, NULL);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }*/

                }

                //if( frame_num <= 10 ){
                    ret = av_buffersrc_add_frame_flags(*mainsrc_ctx, frame,AV_BUFFERSRC_FLAG_PUSH);
                    if(ret < 0){
                        printf("Error: av_buffersrc_add_frame failed\n");
                        return ret;
                    }
                //}
                ret = av_buffersink_get_frame_flags(*resultsink_ctx, frame,AV_BUFFERSINK_FLAG_NO_REQUEST);
                if( ret == AVERROR_EOF ){
                    //没有更多的 AVFrame
                    printf("no more avframe output \n");
                }else if( ret == AVERROR(EAGAIN) ){
                    //需要输入更多的 AVFrame
                    printf("need more avframe input \n");
                }


                //滤镜处理结束


  
    frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
    int ret_enc = avcodec_send_frame(*enc_ctx, frame);
//printf("return code %d",ret_enc);
    if (ret_enc < 0) {
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
        continue;
    }

    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
//printf("%s: pts:%"PRId64,"out_pkt",out_pkt->pts);
//printf("%s: dts:%"PRId64,"out_pkt",out_pkt->dts);
          if (ret_enc == AVERROR(EAGAIN)) {
        //printf("$$$$$$$222222$$$$$$$$$$");
 //printf("pkt_out size : %d \n",out_pkt->size);

break;

      }
else if( ret_enc == AVERROR_EOF){
            break;
      }
        else if (ret_enc < 0) {
            fprintf(stderr, "Error during encoding\n");
            return 1;
        }
if (out_pkt->data!=NULL) {
      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }
    //输出流
    //
    ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);
    if(ret_enc<0){
        printf("interleaved write error:%d",ret_enc);
        return ret_enc;
    }

    }


    
    }
    return 0;
  }else{
      return 1;
  } 

}

int simple_interleaved_write_frame_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int *stream_mapping){
 AVStream *out_stream;
 int ret;
//log_packet(ofmt_ctx, out_pkt, "in-2");
 pkt->stream_index = stream_mapping[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];

        out_pkt->stream_index=pkt->stream_index;
//printf("origign_pts:%"PRId64" origign_dts:%"PRId64" origign_duration:%"PRId64"\n",origin_pts,origin_dts,origin_duration);
        out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, input_streams[pkt->stream_index]->st->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, input_streams[pkt->stream_index]->st->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        out_pkt->duration = av_rescale_q_rnd(pkt->duration, input_streams[pkt->stream_index]->st->time_base,  out_stream->time_base,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);

        //out_pkt->pos=origin_pos;
        
      //av_packet_rescale_ts(out_pkt,in_stream->time_base, out_stream->time_base);
      
        //out_pkt->pos = -1;
        //out_pkt-
         log_packet(ofmt_ctx, out_pkt, "out");

        ret = av_interleaved_write_frame(ofmt_ctx, out_pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
 //av_packet_unref(pkt);
        av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_frame_unref(frame);


return ret;
  //          break;
        }
//}  
        av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_frame_unref(frame);

        return 0;

}
int main(int argc, char **argv)
{


 /* while(1){
int64_t current_timestap=av_gettime_relative_customer();
printf("now:%"PRId64"\n",current_timestap);

  }*/



  
      


    av_log_set_level(AV_LOG_DEBUG);
    if (argc < 4) {
        printf("usage: %s input output\n"
               "API example program to remux a media file with libavformat and libavcodec.\n"
               "The output format is guessed according to the file extension.\n"
               "\n", argv[0]);
        return 1;
    }
    const char *in_filename, *subtitle_filename, *rtsp_path;
    AVFilterContext **logo_ctx;
    int ret;
    bool if_hw=true;
    in_filename  = argv[1];
    subtitle_filename =argv[2];
    rtsp_path = argv[3];
     //printf("##################");
     //
       AVRational logo_tb = {0};
                    AVRational logo_fr = {0};
AVFrame *logo_frame = get_frame_from_jpeg_or_png_file2("/workspace/ffmpeg/FFmpeg/doc/examples/laoflch-mc-log.png",&logo_tb,&logo_fr);
//AVFrame *logo_frame = get_frame_from_jpeg_or_png_file2("/home/laoflch/Documents/laoflch-mc-log22.png",&logo_tb,&logo_fr);
    ret=push_video_to_rtsp_subtitle_logo(in_filename, subtitle_filename, &logo_frame,rtsp_path,if_hw,true,480,480*6,480*3);

   
    return 0;
}


int push_video_to_rtsp_subtitle_logo(const char *video_file_path,const char *subtitle_file_path,AVFrame **logo_frame,const char *rtsp_push_path,bool if_hw,bool if_logo_fade,uint64_t duration_frames,uint64_t interval_frames,uint64_t present_frames){
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = avformat_alloc_context(), *ofmt_ctx = NULL;
    AVBufferRef *hw_device_ctx =NULL;
    AVPacket *pkt=av_packet_alloc();
    
    AVPacket *out_pkt=av_packet_alloc();
    AVFrame *frame=av_frame_alloc();
    //AVFrame
    AVFilterGraph **filter_graph= (AVFilterGraph **)malloc(sizeof(AVFilterGraph *));
    *filter_graph=NULL;
    AVFilterContext **mainsrc_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));

    //if(logo_ctx==NULL){
        //logo_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));

    //}
    AVFilterContext **logo_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));
    //logo_ctx=(AVFilterContext **)malloc(sizeof(AVFilterContext **));

    AVFilterContext **resultsink_ctx = (AVFilterContext **)malloc(sizeof(AVFilterContext **));
   
    FilterGraph *filter_graph_des = av_mallocz(sizeof(*filter_graph_des));

    

    filter_graph_des->subtitle_path=av_mallocz(strlen(subtitle_file_path));
    filter_graph_des->current_frame_pts=0;
    //printf("ssssssss%s",subtitle_file_path);

    strcpy(filter_graph_des->subtitle_path, subtitle_file_path);
    filter_graph_des->logo_frame=logo_frame;
    
    filter_graph_des->if_hw=if_hw;
    filter_graph_des->if_fade=if_logo_fade;

    filter_graph_des->duration_frames=duration_frames;
    filter_graph_des->interval_frames = interval_frames;
    filter_graph_des->present_frames=present_frames;
    //filter_graph_des->hide_frames=hide_frames;
    // strlen(subtitle_file_path));
    //filter_graph_des->subtitle_path=subtitle_file_path;
//return -1;
    //const char *in_filename, *out_filename;
    int ret, i;
    int64_t origin_pts,origin_dts,origin_duration,origin_pos;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    bool ifcodec=true;//是否解编码
    //bool if_hw=true;
    const AVCodec *codec;
     int audio_pts=0;
    AVCodecContext *dec_ctx= NULL,*enc_ctx= NULL;
    enum AVHWDeviceType type;
    /*视频编码处理函数
      */
    int (*handle_video_codec)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *);  

    /*视频滤镜处理函数
      */
    int (*handle_video_complex_filter)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *,AVFilterGraph **,AVFilterContext **,AVFilterContext **,AVFilterContext **,FilterGraph *);  



    /*音频编码处理函数
     * */
    int (*handle_audio_codec)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *,int); 

    /*帧输出处理函数
     * */
    int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *);  

    bool rate_emu = true;



    av_log_set_level(AV_LOG_DEBUG);
    //in_filename  = argv[1];
    //out_filename = argv[2];

    //handle_video_codec=simple_video_codec_func;
    handle_video_codec=hw_video_codec_func;
    handle_audio_codec=simple_audio_codec_func;

    handle_interleaved_write_frame=simple_interleaved_write_frame_func;

    //handle_video_complex_filter=complex_filter_video_codec_func;
    handle_video_complex_filter=subtitle_logo_video_codec_func;
    ifmt_ctx->interrupt_callback=int_cb;

    if(if_hw){
        type = av_hwdevice_find_type_by_name("cuda");
        if (type == AV_HWDEVICE_TYPE_NONE) {
            fprintf(stderr, "Device type %s is not supported.\n", "cuda");
            fprintf(stderr, "Available device types:");
            while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
                fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
            fprintf(stderr, "\n");
        }
    }
    if ((ret = avformat_open_input(&ifmt_ctx, video_file_path, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", video_file_path);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, video_file_path, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, "RTSP", rtsp_push_path);
    
    //avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);

    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    AVDictionary* options = NULL;
 
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    //av_opt_set(ofmt_ctx->priv_data,"rtsp_transport","tcp",0);

    //ofmt_ctx->max_interleave_delta = 10000000;

    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
      stream_mapping[i] = -1;

//选择视频流和音频流
      if (i!=0&&i!=1){
       continue;
      }
        GROW_ARRAY(input_streams, nb_input_streams);
        printf("i: %d nb_input_streams %d \n",i,nb_input_streams);
        //add input stream des
              add_input_streams(ifmt_ctx,i,nb_input_streams-1,if_hw,type,hw_device_ctx);
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
//printf("stream time base:{%d %d}\n",in_stream->time_base.den,in_stream->time_base.num);

//printf("stream time base:{%d %d} i:%d\n",input_streams[i]->dec_ctx->time_base.den,input_streams[i]->dec_ctx->time_base.num,i);

                           
        /*if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                       continue;
        }*/

       // stream_mapping[i] = stream_index++;
        stream_mapping[i]=nb_input_streams-1;
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        //AVCodec *encode = avcodec_find_encoder(dec_ctx->codec_id);
        //enc_ctx = avcodec_alloc_context3(encode);
        //if (!enc_ctx) {
        //    fprintf(stderr, "Could not allocate video codec context\n");
        //    return 1; 
        //a
        //}
        //
        //
        //
     
        if(ifcodec){
        GROW_ARRAY(output_streams, nb_output_streams);
OutputStream *output_stream;
if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
        output_stream = new_output_stream(ofmt_ctx, nb_output_streams-1, in_codecpar->codec_type,86018);
}else{
 output_stream = new_output_stream(ofmt_ctx, nb_output_streams-1, in_codecpar->codec_type,in_codecpar->codec_id);

}
        if (!output_stream) {
            fprintf(stderr, "Could not allocate output stream\n");
            return 1; 
        
        }
   
        enc_ctx=output_stream->enc_ctx;

        if (in_codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
//AVCodec *encode = avcodec_find_encoder(AV_CODEC_ID_H264);
                    //
                    //AVCodecParameters *in_codecpar = fmt_ctx->streams[0]->codecpar;

    //    enc_ctx = avcodec_alloc_context3(encode);
     //   if (!enc_ctx) {
      //      fprintf(stderr, "Could not allocate video codec context\n");
       //     return 1; 
       //
       //     
        //}
                    //``enc_ctx->bit_rate = 4000000;
                    //enc_ctx->framerate = input_streams[i]->dec_ctx->framerate;
                    //enc_ctx->gop_size = 10;
                    //enc_ctx->max_b_frames = 1;

                  //enc_ctx->time_base= 
                    //enc_ctx->framerate =input_streams[i]->dec_ctx->framerate;
                    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;

                    enc_ctx->time_base = input_streams[nb_input_streams-1]->dec_ctx->time_base;
                    
//printf("stream time base:{%d %d}\n",enc_ctx->time_base.den,enc_ctx->time_base.num);
                    enc_ctx->width = input_streams[nb_input_streams-1]->dec_ctx->width;//frame->width;//fmt_ctx->streams[0]->codecpar->width;
                    enc_ctx->height = input_streams[nb_input_streams-1]->dec_ctx->height;// frame->height;//fmt_ctx->streams[0]->codecpar->height;
                    enc_ctx->sample_aspect_ratio = input_streams[nb_input_streams-1]->dec_ctx->sample_aspect_ratio;//frame->sample_aspect_ratio;
                    //(*enc_ctx)->pix_fmt = frame->format;
                   if(if_hw){ 
enc_ctx->pix_fmt= AV_PIX_FMT_NV12;
enc_ctx->sw_pix_fmt=AV_PIX_FMT_YUV420P;
                                       }else{
enc_ctx->pix_fmt = input_streams[nb_input_streams-1]->dec_ctx->pix_fmt;

                   }//
                    //enc_ctx->pix_fmt=AV_PIX_FMT_YUV420P;
                    //(*enc_ctx)->channels=3;
                    //(*enc_ctx)->color_range            = frame->color_range;
                    //(*enc_ctx)->color_primaries        = frame->color_primaries;
                    //(*enc_ctx)->color_trc              = frame->color_trc;
                    //(*enc_ctx)->colorspace             = frame->colorspace;
                    //(*enc_ctx)->chroma_sample_location = frame->chroma_location;
 //                   (*enc_ctx)->me_range = 16; 
  //                  (*enc_ctx)->max_qdiff = 4;
   //                 (*enc_ctx)->qmin = 10; 
    //                (*enc_ctx)->qmax = 51; 
     //               (*enc_ctx)->qcompress = 0.6;
                    //
                   if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
                       enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

                    }
//av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
//av_opt_set(enc_ctx->priv_data,"tune","film",0);
                   //av_opt_set(enc_ctx->priv_data,"tune","zerolatency",0);
                          /*ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;*/
        }else if (in_codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
                  enc_ctx->sample_rate=input_streams[nb_input_streams-1]->dec_ctx->sample_rate;

            enc_ctx->channel_layout=input_streams[nb_input_streams-1]->dec_ctx->channel_layout;  
         //enc_ctx->codec->capabilities=AV_CODEC_CAP_VARIABLE_FRAME_SIZE; 
          
            enc_ctx->channels=av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
            //enc_ctx->time_base = (AVRational){1,enc_ctx->sample_rate};
            enc_ctx->time_base = input_streams[nb_input_streams-1]->dec_ctx->time_base;
            enc_ctx->sample_fmt=output_stream->enc_ctx->codec->sample_fmts[0];
           enc_ctx->strict_std_compliance=-2; 
           //printf("dec frame size:%d enc frame size:%d",input_streams[nb_input_streams-1]->dec_ctx->frame_size,enc_ctx->frame_size);
           //printf("cap:%d \n",enc_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE);

           if (((enc_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)==0) && (input_streams[nb_input_streams-1]->dec_ctx->frame_size!=enc_ctx->frame_size)){
            // printf("######################\n");
           output_stream->audio_fifo=av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, 1);
           }//av_opt_set(enc_ctx->priv_data, "2", "", 0);



        }else{
          continue;
        }



//        printf("stream time base:{%d %d}\n",enc_ctx->time_base.den,enc_ctx->time_base.num);
        if ((ret = avcodec_open2(enc_ctx, output_stream->enc, NULL)) < 0) {
           printf("open codec faile %d \n",ret);
           return ret;
        }  

//printf("stream time base2:{%d %d}\n",enc_ctx->time_base.den,enc_ctx->time_base.num);
 //ret = avcodec_parameters_from_context(out_stream->codecpar,enc_ctx );
 //
 //必须在avcodec_open2之后再设置输出流参数;
        ret = avcodec_parameters_from_context(out_stream->codecpar,enc_ctx );
        //out_stream->codecpar->format=AV_PIX_FMT_YUV420P;
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            return ret;
        }
   if (in_codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
               //printf("enc_ctx frame_size:%d",enc_ctx->frame_size);

           if (((enc_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)==0) && (input_streams[nb_input_streams-1]->dec_ctx->frame_size!=enc_ctx->frame_size)){
            // printf("######################\n");
           output_stream->audio_fifo=av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, 1);
           }//av_opt_set(enc_ctx->priv_data, "2", "", 0);


    
    }
    }else{

        //仅有Packet转发
        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            goto end;
        }

    } 
    }
 //printf("###################################out_filename:%s",out_filename);
  //   av_dump_format(ofmt_ctx, 0, out_filename, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, rtsp_push_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", rtsp_push_path);
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    AVStream *in_stream, *out_stream;
    InputStream *ist;




    int64_t start_frame=0;
    while (1) {
        
        if (rate_emu) {
            bool if_read_packet=true;

            for (i = 0; i < nb_input_streams; i++) {

                ist = input_streams[i];
                //printf("dts:%d",ist->dts);
                int64_t pts = av_rescale(ist->dts, 1000000, AV_TIME_BASE);
                int64_t current_timestap=av_gettime_relative();
                //int64_t now = av_gettime_relativer() - ist->start;      
                int64_t now = current_timestap-ist->start;
 //printf("pts:%"PRId64,pts);
  //                  printf("now:%"PRId64"-%"PRId64"=%"PRId64"\n",current_timestap,ist->start,now);
                if (pts > now){
                    if_read_packet=false;
/*if(pts-start_frame>0){
                    printf("skip %d frames",pts-start_frame);
                    printf("pts:%"PRId64,pts);
                    printf("now:%"PRId64"\n",now);
}
                    start_frame=pts;

 */                   break;

                }
            };
            if (!if_read_packet){
                continue;
            }
//printf("begin read frame \n");
        };
        /*if (input_streams[pkt->stream_index]->dec_ctx->time_base.den==48){
        printf("stream time base4:{%d %d} i:%d\n",input_streams[pkt->stream_index]->dec_ctx->time_base.den,input_streams[pkt->stream_index]->dec_ctx->time_base.num,pkt->stream_index);
        goto end;
          
        };*/
        ret = av_read_frame(ifmt_ctx, pkt); 
 //       printf("read frame ret:%d\n",ret); 
if (ret == AVERROR(EAGAIN)) {
           // av_usleep(10000);
            continue;
        }
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }  


        log_packet(ifmt_ctx, pkt, "in");
 //printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
        in_stream  = ifmt_ctx->streams[pkt->stream_index];
        if (pkt->stream_index >= stream_mapping_size ||
            stream_mapping[pkt->stream_index] < 0) {
            av_packet_unref(pkt);

            continue;
        }
//return -1;
//

               if (rate_emu) {
            ist = input_streams[stream_mapping[pkt->stream_index]];
 //printf("input_streams:%d\n",stream_mapping[pkt->stream_index]); 
//            printf("out packet dts:%d\n",pkt.dts);
//printf("pts:%"PRId64,pkt.dts);
//                    printf(" now:%"PRId64,av_gettime_relative() - ist->start);
//printf(" pkt address:%"PRId64,&pkt);
            if (pkt != NULL && pkt->dts != AV_NOPTS_VALUE) {
                ist->next_dts = ist->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, AV_TIME_BASE_Q,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
//printf(" ist-dts:%"PRId64"\n",ist->dts);
            //if (ist->dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO || !ist->decoding_needed)
                ist->next_pts = ist->pts = ist->dts;
            }

        }

origin_pts=pkt->pts;
origin_dts=pkt->dts;
origin_pos=pkt->pos;
origin_duration=pkt->duration;
    
    if (ifcodec&&(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO||ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)){

//log_packet(ifmt_ctx, pkt, "in_3");

      //处理视频编码
        if(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO&&handle_video_codec!=NULL){
          if (pkt->data!=NULL) {
      av_packet_rescale_ts(pkt,input_streams[stream_mapping[pkt->stream_index]]->st->time_base,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base);
    }
 //log_packet(ifmt_ctx,pkt , "in_2");
        //if (0){
            //ret=(*handle_video_codec)(pkt,out_pkt,frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,&output_streams[stream_mapping[pkt->stream_index]]->enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping[pkt->stream_index],handle_interleaved_write_frame,stream_mapping);
     

        //处理滤镜 
          ret=(*handle_video_complex_filter)(pkt,out_pkt,frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,&output_streams[stream_mapping[pkt->stream_index]]->enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping[pkt->stream_index],handle_interleaved_write_frame,stream_mapping,filter_graph,mainsrc_ctx,logo_ctx,resultsink_ctx,filter_graph_des );
           //         if(filter_graph==NULL){

//                    }
 /*if (pkt->data!=NULL) {
      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }
*/
 //log_packet(ifmt_ctx,out_pkt , "out_2");
   //printf("enc time base:{%d %d}",output_streams[pkt->stream_index]->enc_ctx->time_base.den,output_streams[pkt->stream_index]->enc_ctx->time_base.num);

            //av_frame_unref(frame);
            
            if (ret == AVERROR(EAGAIN)) {

               continue;
            }
            else if (ret != 0) {
             fprintf(stderr, "handle video codec faile\n");

             continue;
            }

continue;
        }

//log_packet(ifmt_ctx, pkt, "in_4");

//处理音频编码
        if(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO&&handle_audio_codec!=NULL){
          //continue;
          if (pkt->data!=NULL) {
      av_packet_rescale_ts(pkt,input_streams[stream_mapping[pkt->stream_index]]->st->time_base,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base);
    }
 //log_packet(ifmt_ctx,pkt , "in_3");
        //if (0){
            ret=(*handle_audio_codec)(pkt,out_pkt,frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,&output_streams[stream_mapping[pkt->stream_index]]->enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping[pkt->stream_index],handle_interleaved_write_frame,stream_mapping,audio_pts);

 /*if (pkt->data!=NULL) {
      av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
      }*/
            if (ret == AVERROR(EAGAIN)) {

               continue;
            }
            else if (ret < 0) {
             fprintf(stderr, "handle audio codec faile\n");

             continue;
            }

audio_pts=ret;
            continue;

        }

       
    }else{
      out_pkt=pkt;

 ret=simple_interleaved_write_frame_func(pkt,out_pkt,frame,dec_ctx,&enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping);
    if(ret<0){
        printf("interleaved write error:%d",ret);
        return ret;
    }


    }

 /*log_packet(ifmt_ctx, pkt, "in-2");
 pkt->stream_index = stream_mapping[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];

        out_pkt->stream_index=pkt->stream_index;
//printf("origign_pts:%"PRId64" origign_dts:%"PRId64" origign_duration:%"PRId64"\n",origin_pts,origin_dts,origin_duration);
        out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, input_streams[pkt->stream_index]->st->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, input_streams[pkt->stream_index]->st->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        out_pkt->duration = av_rescale_q_rnd(pkt->duration, input_streams[pkt->stream_index]->st->time_base,  out_stream->time_base,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);

        //out_pkt->pos=origin_pos;
        
      //av_packet_rescale_ts(out_pkt,in_stream->time_base, out_stream->time_base);
      
        //out_pkt->pos = -1;
        //out_pkt-
         //log_packet(ofmt_ctx, out_pkt, "out");

        ret = av_interleaved_write_frame(ofmt_ctx, out_pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
 av_packet_unref(pkt);

            break;
        }*/
//}  
        av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_freep(frame->data[0]);
        av_frame_unref(frame);
        
        
        
    }

    av_write_trailer(ofmt_ctx);
end:
    av_frame_free(&frame);
    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    avfilter_graph_free(filter_graph);

    av_packet_free(&pkt);
    av_packet_free(&out_pkt);
    av_buffer_unref(&hw_device_ctx);
    av_freep(&stream_mapping);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;

}

/**
 * Initialize one input frame for writing to the output file.
 * The frame will be exactly frame_size samples large.
 * @param[out] frame                Frame to be initialized
 * @param      output_codec_context Codec context of the output file
 * @param      frame_size           Size of the frame
 * @return Error code (0 if successful)
 */
int init_audio_output_frame(AVFrame **frame,
                                   AVCodecContext *occtx,
                                   int frame_size)
{
    int error;

    /* Create a new frame to store the audio samples. */
    if (!(*frame = av_frame_alloc()))
    {
        fprintf(stderr, "Could not allocate output frame\n");
        return AVERROR_EXIT;
    }

    /* Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity. */
    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = occtx->channel_layout;
    (*frame)->format         = occtx->sample_fmt;
    (*frame)->sample_rate    = occtx->sample_rate;

    /* Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified. */
    // 为AVFrame分配缓冲区，此函数会填充AVFrame.data和AVFrame.buf，若有需要，也会填充
    // AVFrame.extended_data和AVFrame.extended_buf，对于planar格式音频，会为每个plane
    // 分配一个缓冲区
    if ((error = av_frame_get_buffer(*frame, 0)) < 0)
    {
        fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
                av_err2str(error));
        av_frame_free(frame);
        return error;
    }

    return 0;
}

// FIFO中可读数据小于编码器帧尺寸，则继续往FIFO中写数据
int write_frame_to_audio_fifo(AVAudioFifo *fifo,
                                     uint8_t **new_data,
                                     int new_size)
{
    int ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + new_size);
    if (ret < 0)
    {
        fprintf(stderr, "Could not reallocate FIFO\n");
        return ret;
    }
    
    /* Store the new samples in the FIFO buffer. */
    ret = av_audio_fifo_write(fifo, (void **)new_data, new_size);
    if (ret < new_size)
    {
        fprintf(stderr, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }

    return 0;
}

int read_frame_from_audio_fifo(AVAudioFifo *fifo,
                                      AVCodecContext *occtx,
                                      AVFrame **frame)
{
    AVFrame *output_frame;
    // 如果FIFO中可读数据多于编码器帧大小，则只读取编码器帧大小的数据出来
    // 否则将FIFO中数据读完。frame_size是帧中单个声道的采样点数
    const int frame_size = FFMIN(av_audio_fifo_size(fifo), occtx->frame_size);

    /* Initialize temporary storage for one output frame. */
    // 分配AVFrame及AVFrame数据缓冲区
    int ret = init_audio_output_frame(&output_frame, occtx, frame_size);
    if (ret < 0)
    {
        return AVERROR_EXIT;
    }

    // 从FIFO从读取数据填充到output_frame->data中
    ret = av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size);
    if (ret < frame_size)
    {
        fprintf(stderr, "Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    *frame = output_frame;

    return ret;

}
AVFrame* get_frame_from_jpeg_or_png_file2(const char *filename,AVRational *logo_tb,AVRational *logo_fr){
    int ret = 0;

    AVDictionary *format_opts = NULL;
    av_dict_set(&format_opts, "probesize", "5000000", 0);
    AVFormatContext *format_ctx = NULL;
    if ((ret = avformat_open_input(&format_ctx, filename, NULL, &format_opts)) != 0) {
        printf("Error: avformat_open_input failed \n");
        return NULL;
    }
    avformat_find_stream_info(format_ctx,NULL);

    *logo_tb = format_ctx->streams[0]->time_base;
    *logo_fr = av_guess_frame_rate(format_ctx, format_ctx->streams[0], NULL);

    //打开解码器
    AVCodecContext *av_codec_ctx = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(av_codec_ctx, format_ctx->streams[0]->codecpar);
    if (ret < 0){
        printf("error code %d \n",ret);
        avformat_close_input(&format_ctx);
        return NULL;
    }

    AVDictionary *codec_opts = NULL;
    av_dict_set(&codec_opts, "sub_text_format", "ass", 0);
    AVCodec *codec = avcodec_find_decoder(av_codec_ctx->codec_id);
    if(!codec){
        printf("codec not support %d \n",ret);
        return NULL;
    }
    if ((ret = avcodec_open2(av_codec_ctx, codec, NULL)) < 0) {
        printf("open codec faile %d \n",ret);
        return NULL;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    ret = av_read_frame(format_ctx, &pkt);
    ret = avcodec_send_packet(av_codec_ctx, &pkt);
    AVFrame *frame_2 = av_frame_alloc();
    ret = avcodec_receive_frame(av_codec_ctx, frame_2);

    //省略错误处理

    av_dict_free(&format_opts);
    av_dict_free(&codec_opts);

    return frame_2;
}
int hw_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *stream_mapping){



  if(pkt!=NULL){
    AVFrame *sw_frame =NULL; //;
//    AVFrame *tmp_frame = NULL;
//sw_frame =   
    int ret,s_pts;
  printf("dec time base:{%d %d}",dec_ctx->time_base.den,dec_ctx->time_base.num);
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
 
      if (ret == AVERROR(EAGAIN)) {
 return ret;

      }
else if( ret == AVERROR_EOF){
                  return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);
            return 1 ;
        }

//log_packet(fmt_ctx, pkt, "out-1");    /* send the frame to the encoder */
    if (frame!=NULL)
        printf("Send frame %"PRId64" size %"PRId64" aspect{%d %d} \n", frame->pts,frame->linesize,frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num);
    
  //printf("################################");
    s_pts=frame->pts;

    if (frame->format == hw_pix_fmt) {
         sw_frame=av_frame_alloc();

                  /* retrieve data from GPU to CPU */
         if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
               // goto fail;
         }
         sw_frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
         sw_frame->pts=s_pts;

         //av_frame_free(&frame);
         av_frame_unref(frame) ;
        // av_frame_free(&frame);
//ret=av_frame_copy(frame, sw_frame);

         //frame = sw_frame;
    } //else
           // tmp_frame = frame;
   
    int ret_enc = avcodec_send_frame(*enc_ctx, sw_frame);

    if (ret_enc < 0) {
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
        continue;
    }

    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);

          if (ret_enc == AVERROR(EAGAIN)) {

             break;
          }else if( ret_enc == AVERROR_EOF){

            break;
          }else if (ret_enc < 0) {
            fprintf(stderr, "Error during encoding\n");
            return 1;
        }
        if (out_pkt->data!=NULL) {

          //out_pkt->pts=s_pts;
          //out_pkt->dts=s_pts;
          //out_pkt->duration=pkt->duration;
            av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
        }
    //输出流
    //
        ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,sw_frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,stream_mapping);
       av_frame_free(&sw_frame) ;
        
        if(ret_enc<0){
            printf("interleaved write error:%d",ret_enc);
            return ret_enc;
        
        }

    }


    
    }
    return 0;
  }else{
      return 1;
  } 

}

int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type,AVBufferRef *hw_device_ctx){
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return err;
}

int handle_logo_fade(AVFrame *frame,uint64_t duration_frames,uint64_t interval_frames,uint64_t present_frames){
    //FadeContext s = (FadeContext)av_malloc(sizeof(FadeContext));
    //
   uint8_t rgba_map[4];
    const AVPixFmtDescriptor *pixdesc = av_pix_fmt_desc_get(frame->format);

    int hsub = pixdesc->log2_chroma_w;
   // int vsub = pixdesc->log2_chroma_h;
   //
    uint64_t inner_serise=frame->pts%(2*duration_frames+interval_frames);
    int fade_per_frame = (1 << 16) / duration_frames; 

    int factor;

    //printf(" factor :%d %d %d", inner_serise,duration_frames,interval_frames);
    if (inner_serise<duration_frames){
        factor=inner_serise*fade_per_frame;

    }else if(inner_serise<(duration_frames+present_frames)&&inner_serise>=duration_frames){

        factor=duration_frames*fade_per_frame;
     
    }else if(inner_serise<(2*duration_frames+present_frames)&&inner_serise>=duration_frames+present_frames){

        factor=(2*duration_frames+present_frames-inner_serise)*fade_per_frame;


    }else if(inner_serise<(2*duration_frames+interval_frames+present_frames)&&inner_serise>=2*duration_frames+present_frames){

      
        factor=0;
    }

    //factor=duration_frames*fade_per_frame/2;

    printf(" %d \n",factor);
   
    //int factor=360*fade_per_frame;
    int bpp = pixdesc->flags & AV_PIX_FMT_FLAG_PLANAR ?
             1 :
             av_get_bits_per_pixel(pixdesc) >> 3;
    int alpha = !!(pixdesc->flags & AV_PIX_FMT_FLAG_ALPHA);
    uint8_t is_packed_rgb = ff_fill_rgba_map(rgba_map, frame->format) >= 0;

    /* use CCIR601/709 black level for studio-level pixel non-alpha components */
    //unsigned int black_level =
     //       ff_fmt_is_in(frame->format, studio_level_pix_fmts) && !alpha ? 16 : 0;
    /* 32768 = 1 << 15, it is an integer representation
     * of 0.5 and is for rounding. */
    //unsigned int black_level_scaled = (black_level << 16) + 32768;
uint8_t color_rgba[4];//={0,0,0,0};
int ret;
    ret=av_parse_color(color_rgba,"Black",-1,NULL);
    color_rgba[3]=0;

    if      (alpha) {   filter_rgba(frame, 1, frame->height, 1, 4,&rgba_map,&color_rgba,factor);
    }else if (bpp == 3){ filter_rgba(frame, 1, frame->height, 0, 3,&rgba_map,&color_rgba,factor); 
    }else if (bpp == 4) {filter_rgba(frame, 1, frame->height, 0, 4,&rgba_map,&color_rgba,factor); }

    return 0;
}

void filter_rgba( const AVFrame *frame,
                                        int slice_start, int slice_end,
                                        int do_alpha, int step,uint8_t *rgba_map,uint8_t *color_rgba,int factor)
{
    int i, j;
    const uint8_t r_idx  = rgba_map[R];
    const uint8_t g_idx  = rgba_map[G];
    const uint8_t b_idx  = rgba_map[B];
    const uint8_t a_idx  = rgba_map[A];
    const uint8_t *c = color_rgba;

    for (i = slice_start; i < slice_end; i++) {
        uint8_t *p = frame->data[0] + i * frame->linesize[0];
        for (j = 0; j < frame->width; j++) {

         // printf("origin: r(%d) g(%d) b(%d) a(%d)\n",p[r_idx],p[g_idx],p[b_idx],p[a_idx]);
//printf("color rgba: r(%d) g(%d) b(%d) a(%d)\n",c[r_idx],c[g_idx],c[b_idx],c[a_idx]);
//printf("move: (%x)c<<16(%x) 1<<15(%x)\n",c[r_idx],c[r_idx]<<16,1<<15);

#define INTERP(c_name, c_idx) av_clip_uint8(((c[c_idx]<<16) + ((int)p[c_name] - (int)c[c_idx]) * factor + (1<<15)) >> 16)
            p[r_idx] = INTERP(r_idx, 0);
            p[g_idx] = INTERP(g_idx, 1);
            p[b_idx] = INTERP(b_idx, 2);
            if (do_alpha)
                p[a_idx] = INTERP(a_idx, 3);

 //        printf("new: r(%d) g(%d) b(%d) a(%d)\n",p[r_idx],p[g_idx],p[b_idx],p[a_idx]);
            p += step;
        }
    }
}
int ff_fill_rgba_map(uint8_t *rgba_map, enum AVPixelFormat pix_fmt)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);
    if (!(desc->flags & AV_PIX_FMT_FLAG_RGB))
        return AVERROR(EINVAL);
    if (desc->flags & AV_PIX_FMT_FLAG_BITSTREAM)
        return AVERROR(EINVAL);
    //av_assert0(desc->nb_components == 3 + !!(desc->flags & AV_PIX_FMT_FLAG_ALPHA));
    if (desc->flags & AV_PIX_FMT_FLAG_PLANAR) {
        rgba_map[R]   = desc->comp[0].plane;
        rgba_map[G] = desc->comp[1].plane;
        rgba_map[B]  = desc->comp[2].plane;
        rgba_map[A] = (desc->flags & AV_PIX_FMT_FLAG_ALPHA) ? desc->comp[3].plane : 3;
    } else {
        int had0 = 0;
        unsigned depthb = 0;
        unsigned i;
        for (i = 0; i < desc->nb_components; i++) {
            /* all components must have same depth in bytes */
            unsigned db = (desc->comp[i].depth + 7) / 8;
            unsigned pos = desc->comp[i].offset / db;
            if (depthb && (depthb != db))
                return AVERROR(ENOSYS);

            if (desc->comp[i].offset % db)
                return AVERROR(ENOSYS);

            had0 |= pos == 0;
            rgba_map[i] = pos;
        }

        if (desc->nb_components == 3)
            rgba_map[A] = had0 ? 3 : 0;
    }

   /* av_assert0(rgba_map[R]   != rgba_map[G]);
    av_assert0(rgba_map[G] != rgba_map[B]);
    av_assert0(rgba_map[B]  != rgba_map[R]);
    av_assert0(rgba_map[R]   != rgba_map[A]);
    av_assert0(rgba_map[G] != rgba_map[A]);
    av_assert0(rgba_map[B]  != rgba_map[A]);
*/
    return 0;
}
