/*
*/
#include <libavutil/timestamp.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>

#include <libavutil/bprint.h>
#include <libavutil/time.h>//必须引入，否则返回值int64_t无效，默认返回int,则会出现负值情况
#include <libavutil/audio_fifo.h>
#include <libavutil/hwcontext.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
//#include <libavfilter/drawutils.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/parseutils.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>


#include "complex_filter.h"
#include "push_stream.h"
#include "subtitle.h"
#include "ass/ass.h"
#include <ass/ass_types.h>

#define ENCODE_ALIGN_BITS 0x40
#define R 0
#define G 1
#define B 2
#define A 3
#define AR(c)  ( (c)>>24)
#define AG(c)  (((c)>>16)&0xFF)
#define AB(c)  (((c)>>8) &0xFF)
#define AA(c)  ((0xFF-(c)) &0xFF)

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts){
    /*const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;*/
const enum AVPixelFormat *p;

 for (p = pix_fmts; *p != -1; p++) {
  printf("##################  %d %s \n",*p,av_get_pix_fmt_name(*p));
 }


  if(ctx!=NULL){

 printf("##################  %d %s \n",AV_PIX_FMT_CUDA,av_get_pix_fmt_name(AV_PIX_FMT_CUDA));
    return AV_PIX_FMT_CUDA;

  }

  //return AV_PIX_FMT_NONE;

}

void add_input_stream_cuda( AVFormatContext *ic, int stream_index,int input_stream_index,bool hw,enum AVHWDeviceType type,AVBufferRef **hw_device_ctx,InputStream **input_streams )
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
        const AVCodecDescriptor *dec_desc;

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
        ist->start_time=av_gettime_relative();

        //}
        //
        if(ic->start_time>0){
            ist->start_pts=av_rescale_q(ic->start_time,AV_TIME_BASE_Q,st->time_base);
        }else{
            ist->start_pts=0;
        }
        ist->ts_scale = 1.0;
        //AVCodecParameters *in_codecpar = ifmt_ctx->streams[0]->codecpar;
//AVRational time_base= st->time_base;
//
        if(hw&&par->codec_type==AVMEDIA_TYPE_VIDEO&&type== AV_HWDEVICE_TYPE_CUDA){

          switch(par->codec_id){
            case AV_CODEC_ID_HEVC:

               ist->dec = avcodec_find_decoder_by_name("hevc_cuvid");
               //可以有hevc 和 hevc_cuvid两种解码器解码，都能使用nv的显卡进行解码，但hevc_cuvid的解码器有bug，对有B帧的视频无法完美解码,解出的帧frame->pict_type都是0。解码有B帧的视频还是要使用"hevc"解码器.
               //ist->dec = avcodec_find_decoder_by_name("hevc_cuvid");
               break;

            case AV_CODEC_ID_H264:

               ist->dec = avcodec_find_decoder_by_name("h264_cuvid");
               break;

            default:
               ist->dec = avcodec_find_decoder(par->codec_id);

          }

           
        }else {
            ist->dec = avcodec_find_decoder(par->codec_id);

        }

           //ist->dec = avcodec_find_decoder_by_name("hevc_cuvid");

        if (!ist->dec) {
            fprintf(stderr, "Could not allocate video decodec \n");
            return ; 
        }

//硬解码
        /*if(hw&&par->codec_type==AVMEDIA_TYPE_VIDEO){
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
*/


        ist->dec_ctx = avcodec_alloc_context3(ist->dec);
        if (!ist->dec_ctx) {
            fprintf(stderr, "Could not allocate video decodec context\n");
            return ; 
        }
 //printf("decodec_id:%d sample_fmt%d channel_layout: %"PRId64"\n",ist->dec_ctx->codec_id,ist->dec_ctx->sample_fmt,av_get_default_channel_layout(8));
  //      printf("AVStream time_base:{%d %d} bit_rate:%d\n",st->time_base.den,st->time_base.num,st->codecpar->bit_rate);
        
        ret = avcodec_parameters_to_context(ist->dec_ctx, par);
        if (ret < 0){
            printf("error code %d \n",ret);
            return ;
        }
 //printf("decodec_id:%d sample_fmt%d\n",ist->dec_ctx->codec_id,ist->dec_ctx->sample_fmt);
        ist->dec_ctx->pkt_timebase=st->time_base;
        ist->dec_ctx->time_base=st->time_base;
        ist->dec_ctx->framerate=av_guess_frame_rate(ic,st,NULL);

        if(hw&&par->codec_type==AVMEDIA_TYPE_VIDEO){ 
            ist->dec_ctx->get_format  = get_hw_format;

            if (hw_decoder_init(ist->dec_ctx, type,hw_device_ctx) < 0)
            return ;
        }



  //      printf("decodec_id:%d sample_fmt%d\n",ist->dec_ctx->codec_id,ist->dec_ctx->sample_fmt);
 //printf("AVdecodecctx time_base:{%d %d} \n",ist->dec_ctx->time_base.den,ist->dec_ctx->time_base.num);
        //printf("AVdecodecctx time_base:{%d %d} \n",ist->dec_ctx->time_base.den,ist->dec_ctx->time_base.num);
          if (par->codec_type== AVMEDIA_TYPE_VIDEO||par->codec_type== AVMEDIA_TYPE_AUDIO){

   ist->dec_ctx->thread_count=8;
   //av_log(ist->dec_ctx, AV_LOG_INFO, "codec capabilities:%d",ist->dec_ctx->codec->capabilities);
            ist->dec_ctx->active_thread_type=FF_THREAD_SLICE;
//if(!av_dict_get(ist->decoder_opts,"threads",NULL,0))
   av_dict_set(&ist->decoder_opts,"threads","8",0);
          }

        if(par->codec_type==AVMEDIA_TYPE_VIDEO){

            ist->dec_ctx->pix_fmt=AV_PIX_FMT_CUDA;
            //ist->dec_ctx->sw_pix_fmt=AV_PIX_FMT_NV12;

            //ist->dec_ctx->thread_count=8;
   //av_log(ist->dec_ctx, AV_LOG_INFO, "codec capabilities:%d",ist->dec_ctx->codec->capabilities);
            //ist->dec_ctx->active_thread_type=FF_THREAD_SLICE;
        }

        if(par->codec_type==AVMEDIA_TYPE_SUBTITLE){

            dec_desc = avcodec_descriptor_get(st->codecpar->codec_id);
            if (dec_desc && (dec_desc->props & AV_CODEC_PROP_TEXT_SUB)){
       
    //if (ass->charenc)
     //   av_dict_set(&codec_opts, "sub_charenc", ass->charenc, 0);
                if (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,26,100)){
                    av_dict_set(&(ist->decoder_opts), "sub_text_format", "ass", 0);
        
                }
            }


        }



      // printf("1 is vs dec_ctx time_base:{%d %d} {%d %d} %d %d %d \n",ic->streams[input_stream_index]->time_base.den,ic->streams[input_stream_index]->time_base.num,input_streams[input_stream_index]->dec_ctx->time_base.den,input_streams[input_stream_index]->dec_ctx->time_base.num,input_streams[input_stream_index],ist,input_stream_index);
    

        if ((ret = avcodec_open2(ist->dec_ctx, ist->dec, &ist->decoder_opts)) < 0) {
//if ((ret = avcodec_open2(ist->dec_ctx, ist->dec, NULL)) < 0) {

           av_log(NULL,"open codec faile %d \n",ret);
           return ;
        }

        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@2 %d \n",(*hw_device_ctx)->buffer);
  
   //printf("2 is vs dec_ctx time_base:{%d %d} {%d %d} %d %d %d \n",ic->streams[input_stream_index]->time_base.den,ic->streams[input_stream_index]->time_base.num,input_streams[input_stream_index]->dec_ctx->time_base.den,input_streams[input_stream_index]->dec_ctx->time_base.num,input_streams[input_stream_index],ist,input_stream_index);
    //    printf("dec_ctx:%d sample_fmt:%d \n",ist->dec_ctx->frame_size,ist->dec_ctx->sample_fmt);

        //


 //printf("AVdecodecctx time_base:{%d %d} \n",ist->dec_ctx->time_base.den,ist->dec_ctx->time_base.num);


        //MATCH_PER_STREAM_OPT(ts_scale, dbl, ist->ts_scale, ic, st);
       
        //guess_input_channel_layout(ist);

}

OutputStream *add_output_stream_cuda(AVFormatContext *oc,  int output_stream_index,bool hw,int codec_type,enum AVCodecID codec_id,OutputStream **output_streams )
{

    if(codec_type==AVMEDIA_TYPE_SUBTITLE){
        return NULL;
    };
    OutputStream *ost;
    //AVStream *st = avformat_new_stream(oc, NULL);
    AVStream *st = oc->streams[output_stream_index];

    AVCodecParameters *par = st->codecpar;

    int idx      = oc->nb_streams - 1, ret = 0;
    const char *bsfs = NULL, *time_base = NULL;
    char *next, *codec_tag = NULL;
    double qscale = -1;
    //int i;


    
    //GROW_ARRAY(output_streams, nb_output_streams);
    if (!(ost = av_mallocz(sizeof(*ost))))
        return NULL;

    output_streams[output_stream_index] = ost;

    //ost->file_index = nb_output_files - 1;
    ost->index      = idx;
    //ost->st         = st;
    ost->forced_kf_ref_pts = AV_NOPTS_VALUE;

    ost->audio_fifo=NULL;

    //if(codec_type== AVMEDIA_TYPE_AUDIO&&if_audio_fifo){
    //ost->audio_fifo=av_mallocz(sizeof(AVAudioFifo *));
    //};
   //oc->streams[1]->codecpar-> 
    //st->codecpar->codec_type = codec_type;
    //printf("encodec_id:%d",codec_id);
    //
   if (codec_type== AVMEDIA_TYPE_VIDEO&&hw){

printf("codec_id:%d hevc:%d h264:%d \n",codec_id,AV_CODEC_ID_HEVC,AV_CODEC_ID_H264);
          switch(codec_id){
            case AV_CODEC_ID_HEVC:


//printf("##############3")
               ost->enc = avcodec_find_encoder_by_name("hevc_nvenc");
               break;
            case AV_CODEC_ID_H264:

               ost->enc = avcodec_find_encoder_by_name("h264_nvenc");
               break;

            default:
               ost->enc = avcodec_find_encoder(codec_id);

          }


           // ost->enc = avcodec_find_encoder_by_name("hevc_nvenc");
   //ost->enc_ctx->thread_count=6;
   //av_log(ost->enc_ctx, AV_LOG_INFO, "codec capabilities:%d",ost->enc_ctx->codec->capabilities);
   //ost->enc_ctx->active_thread_type=FF_THREAD_SLICE;
    }else if (codec_type== AVMEDIA_TYPE_AUDIO){

//printf("audio codec_id:%d  \n",codec_id);
        ost->enc = avcodec_find_encoder(codec_id);

    }else{
        printf("audio codec_id:%d  \n",codec_id);
        ost->enc = avcodec_find_encoder(codec_id);
    }
    if (!ost->enc) {
        fprintf(stderr, "Could not allocate video encodec \n");
        return NULL; 
    }

    ost->enc_ctx = avcodec_alloc_context3(ost->enc);


    if (!ost->enc_ctx) {
        fprintf(stderr, "Could not allocate video encodec context\n");
        return NULL; 
    }

    printf("sample_fmt:%d par sample_fmt:%d bit rate:%d \n",ost->enc_ctx->sample_fmt,par->format,par->bit_rate);

    printf("audio stream time  sample_rate:%d channel_layout:%d channels:%d\n",par->sample_rate,par->channel_layout,par->channels);
    ret = avcodec_parameters_to_context(ost->enc_ctx, par);
    if (ret < 0){
            printf("error code %d \n",ret);
            return NULL;
    }
    printf("sample_fmt:%d par sample_fmt:%d\n",ost->enc_ctx->sample_fmt,par->format);
    if (codec_type== AVMEDIA_TYPE_VIDEO||codec_type== AVMEDIA_TYPE_AUDIO){
    //    ost->enc = avcodec_find_encoder_by_name("h264_nvenc");
       ost->enc_ctx->thread_count=8;
   //av_log(ost->enc_ctx, AV_LOG_INFO, "codec capabilities:%d",ost->enc_ctx->codec->capabilities);
       ost->enc_ctx->active_thread_type=FF_THREAD_SLICE;
//if(!av_dict_get(ost->encoder_opts,"threads",NULL,0))
   av_dict_set(&(ost->encoder_opts),"threads","8",0);
    }
//printf("**********************************88 %d \n",ost);

    return ost;
}

int init_filter_graph_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **input_streams,int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **resultsink_ctx,FilterGraph *filter_graph_des,OutputStream **output_streams ){
          AVFilterInOut *inputs, *cur, *outputs;
          int ret;

          if(  *filter_graph==NULL ){
               AVFilterGraph *filter_graph_point;

               AVFilterContext *mainsrc_ctx_point,*subtitle_ctx_point,*logo_ctx_point,*resultsink_ctx_point ;
               //初始化滤镜容器
               filter_graph_point = avfilter_graph_alloc();

               *filter_graph=filter_graph_point;
               if (!filter_graph) {
                   printf("Error: allocate filter graph failed\n");
                   return -1;
               }

               filter_graph_point->nb_threads=8;
               filter_graph_point->thread_type=FF_THREAD_SLICE;

                    // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                                  //time_base 必须与输入的dec_ctx的time一致，字幕才能正常显示
               AVRational tb = dec_ctx->time_base;
               AVRational fr = dec_ctx->framerate;//av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[0], NULL);
                    //AVRational logo_sar = logo_frame->sample_aspect_ratio;
               AVRational sar = frame->sample_aspect_ratio;

               AVBPrint args;
               av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);

               av_bprintf(&args,
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[main];"
                              // "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[logo];"
                               //"[main][logo]overlay_cuda=x=%d:y=%d[result];"
                               //"[result]format=yuv420p[result_2];" 
                               //"[main]scale_cuda=%d:%d:format=cuda[result];"
                               "[main]buffersink",
                               3840, 1616, AV_PIX_FMT_CUDA, tb.num,tb.den,sar.num, sar.den,fr.num, fr.den//,3840,1616
                              // logo_frame->width,logo_frame->height,logo_frame->format,1001,48000,2835,2835,24000,1001,
                               //sw_frame->width-logo_frame->width,sw_frame->height-logo_frame->height,
                               //(*enc_ctx)->width,(*enc_ctx)->height)
                 );
               ret = avfilter_graph_parse2(filter_graph_point, args.str, &inputs, &outputs);
               if (ret < 0) {
                   printf("Cannot parse2 graph\n");
                   return ret;
               }
               
             //  printf("**********************************88 %d \n",dec_ctx->hw_device_ctx);
               //hw_device_setup_for_filter(filter_graph_point, input_streams[0]->dec_ctx->hw_device_ctx);
               //
               //
               //
for(int i=0;i<filter_graph_point->nb_filters;i++){
    filter_graph_point->filters[i]->hw_device_ctx=av_buffer_ref(dec_ctx->hw_device_ctx);

}
               //正式打开滤镜
               ret = avfilter_graph_config(filter_graph_point, NULL);

               if (ret < 0) {
                   printf("Cannot configure graph\n");
                   return ret;
               }
               avfilter_inout_free(&inputs);
               avfilter_inout_free(&outputs);


  //根据 名字 找到 AVFilterContext
               mainsrc_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");
               *mainsrc_ctx=mainsrc_ctx_point;
 //printf("**********************************88 %d \n",filter_graph_point);



              // logo_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");
               //*logo_ctx=logo_ctx_point;
               resultsink_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffersink_1");
               *resultsink_ctx=resultsink_ctx_point;



/*处理ass,srt 字幕
 *
 * 初始化AssContext*/

       /*        if (filter_graph_des->subtitle_path!=NULL&&strcmp(filter_graph_des->subtitle_path,"")>0&&filter_graph_des->ass==NULL){

                   filter_graph_des->ass = (AssContext *)av_malloc(sizeof(AssContext));
                   filter_graph_des->ass->filename=filter_graph_des->subtitle_path;
                   filter_graph_des->ass->stream_index=-1;
                   filter_graph_des->ass->charenc=filter_graph_des->subtitle_charenc;
                   filter_graph_des->ass->force_style=NULL;
                   filter_graph_des->ass->fontsdir=NULL;
                   filter_graph_des->ass->original_w=0;
                   filter_graph_des->ass->alpha=0;

                   if(endWith(filter_graph_des->ass->filename,LAOFMC_SUB_FILE_ASS_SUFFIX)){
                       init_ass(filter_graph_des->ass);

                   }else{
                       init_subtitles(filter_graph_des->ass);

                   }

                   config_input(filter_graph_des->ass, AV_PIX_FMT_YUV420P,  (*enc_ctx)->width,(*enc_ctx)->height);

               }
               */

/*
 *文件内部字幕流处理
 */
    /*                if(filter_graph_des->sub_frame&&filter_graph_des->subtitle_stream_index>=0){*/
                   /*处理pgs 字幕
                    *
                    * 初始化OverlayContext*/
                     /*   filter_graph_des->overlay_ctx=init_overlay_context(AV_PIX_FMT_YUV420P);

                    }else if(filter_graph_des->subtitle_stream_index>=0){
*/
/*处理ass,srt 字幕
 *
 * 初始化AssContext*/
                /*        filter_graph_des->ass = (AssContext *)av_malloc(sizeof(AssContext));
                        filter_graph_des->ass->stream_index=filter_graph_des->subtitle_stream_index;
                        filter_graph_des->ass->charenc=NULL;
                        filter_graph_des->ass->force_style=NULL;
                        filter_graph_des->ass->fontsdir=NULL;
                        filter_graph_des->ass->original_w=0;
                        filter_graph_des->ass->alpha=0;

                       //动态加载字幕文件
                        init_subtitles_dynamic(filter_graph_des->ass,filter_graph_des->subtitle_dec_ctx);
                        //init_subtitles(filter_graph_des->ass);
                        //初始化配置ASS
                        config_ass(filter_graph_des->ass, AV_PIX_FMT_YUV420P,  (*enc_ctx)->width,(*enc_ctx)->height);
                    }

           */
                }


}
int handle_subtitle2(AVFrame **frame,int64_t pts,AssContext *ass,AVRational time_base,bool *if_empty_subtitle){
//AVFilterContext *ctx = inlink->dst;
 //   AVFilterLink *outlink = ctx->outputs[0];
    //AssContext *ass = ctx->priv;
    int detect_change = 0;
//if(frame){
    double time_ms = pts * av_q2d(time_base) * 1000;
    //double time_ms=55220;
  //printf("*************889 %d %d %f %f %d %"PRId64"  \n",time_base.num,time_base.den,av_q2d(time_base),time_ms,ass->track->n_events,pts);
   if(ass&&ass->renderer&&ass->track) {
 //printf("*************889 %d \n",ass->renderer);
    ASS_Image *image = ass_render_frame(ass->renderer, ass->track, time_ms, &detect_change);

       if(image){

    if (detect_change){
        av_log(NULL, AV_LOG_DEBUG, "Change happened at time ms:%f\n", time_ms);

    //AVFrame *frame=gen_empty_layout_frame(100, 100);a
   //frame->width=image->w;
   //frame->height=image->h;
                //tmp_frame->format=logo_frame->format;
    //            frame->format=AV_PIX_FMT_YUVA420P;

   //int ret= gen_empty_layout_frame(frame,image->w,image->h);
   //
       
  //printf("subtitle x:%d y:%d\n",*frame,image->dst_y);
    //overlay_ass_image_cuda(ass, *frame, image);
  //                  printf("uuuuuuuuuuuuuuuuuuuuuuuuu %d %d \n",*frame, frame);
if(!(*if_empty_subtitle) && *frame){
  //printf("uuuuuuuuuuuuuuuuuuuuuuuuu2 %d %d \n",*frame, frame);

  av_frame_unref(*frame);
  av_frame_free(frame);
}
int ret=blend_ass_image(ass, frame, image);

convert_rgba_to_yuva420p(frame);
   // return ret;
    }
*if_empty_subtitle=false;
 //printf("*************889 %d %d \n",frame,*frame);
return 0;

    
       }else{
//frame->width=100;
 //  frame->height=100;
                //tmp_frame->format=logo_frame->format;
  //              frame->format=AV_PIX_FMT_YUVA420P;

 //int ret= gen_empty_layout_frame(frame,100,100);
 //
   *if_empty_subtitle=true;
return 0;
   }


   }else{
 //int ret= gen_empty_layout_frame(frame,100,100);
return 0;
   }
    return -1;

//}else{
//printf("gen empty layout fail frame is empty \n");  
//}
}


int blend_ass_image(AssContext *ass,AVFrame **frame, ASS_Image *img)
{
    int cnt = 0;

    AVFrame *tmp_frame=NULL;
//printf("addres frame:%d %d\n",*frame,frame);
    //img=img->next->next;
    /*while (img) {

      printf("image size:%d %d \n",img->w,img->h);

      
      if (!tmp_frame){
image_t *img_empty=gen_image(img->stride,img->h);
        tmp_frame=av_frame_alloc();
                tmp_frame->width=img->stride;
                tmp_frame->height=img->h;
                //tmp_frame->format=logo_frame->format;
                tmp_frame->format=AV_PIX_FMT_RGBA;
                int ret=av_image_fill_arrays(tmp_frame->data,tmp_frame->linesize,img_empty->buffer,tmp_frame->format,tmp_frame->width,tmp_frame->height,1);
 printf("stride size:%d %d \n",tmp_frame->linesize[0],img->stride);


                if(ret<0){

                  printf("blend ass image faile \n");

                }
      }else{
 uint8_t rgba_color[] = {AR(img->color), AG(img->color), AB(img->color), AA(img->color)};
        FFDrawColor color;
        ff_draw_color(&ass->draw, &color, rgba_color);
        ff_blend_mask(&ass->draw, &color,
                      tmp_frame->data, tmp_frame->linesize,
                      tmp_frame->width, tmp_frame->height,
                      img->bitmap, img->stride, img->w, img->h,
                      3, 0, 0, 0);
 printf("stride size:%d %d \n",tmp_frame->linesize[0],img->stride);
      }
        //blend_single(frame, img);
        ++cnt;

        //if(cnt==2)break;
        img = img->next;
    }
    if(img)free(img);*/

  int max_stride,max_height=0;

  ASS_Image *img_tmp=img;


   while(img_tmp){

     max_stride=FFMAX(max_stride, img_tmp->stride);
     max_height=FFMAX(max_height, img_tmp->h);

    img_tmp=img_tmp->next; 

   }
 //printf("max stride size:%d %d \n",max_stride,max_height);

image_t *img_empty=gen_image(max_stride,max_height);

tmp_frame=av_frame_alloc();
                tmp_frame->width=img_empty->width;
                tmp_frame->height=img_empty->height;
                //tmp_frame->format=logo_frame->format;
                tmp_frame->format=AV_PIX_FMT_RGBA;
blend(img_empty, img);
                int ret=av_image_fill_arrays(tmp_frame->data,tmp_frame->linesize,img_empty->buffer,tmp_frame->format,tmp_frame->width,tmp_frame->height,1);
 //printf("stride size:%d %d \n",tmp_frame->linesize[0],img->stride);
         //free(img_empty->buffer);
         free(img_empty);
                 //free(img);



    *frame=tmp_frame;
    //printf("%d images blended\n", cnt);

    return 0;
}

int gen_empty_layout_frame_yuva420p(AVFrame **frame,int width, int height)
{

   int ret;
//if (*frame)
 //  if(frame){
   image_t *img=gen_image(width,height);
   AVFrame *tmp_frame;
                tmp_frame=av_frame_alloc();
 
                

                tmp_frame->width=width;
                tmp_frame->height=height;
                //tmp_frame->format=logo_frame->format;
                tmp_frame->format=AV_PIX_FMT_RGBA;
                ret=av_image_fill_arrays(tmp_frame->data,tmp_frame->linesize,img->buffer,tmp_frame->format,tmp_frame->width,tmp_frame->height,1);
                //free(img->buffer);
              // printf("yuva420p1 w:%d h:%d\n",(tmp_frame)->width,(tmp_frame)->height);
               *frame=tmp_frame; 
                //
                //
                //
               convert_rgba_to_yuva420p(frame) ;
                   return 0;


}

int convert_rgba_to_yuva420p(AVFrame **frame){
          int ret;
          AVFrame *yuva_frame=av_frame_alloc();

          yuva_frame->width=(*frame)->width;
          yuva_frame->height=(*frame)->height;
          yuva_frame->format=AV_PIX_FMT_YUVA420P;
          printf("yuva420p w:%d h:%d\n",(*frame)->width,(*frame)->height);
          ret=av_frame_get_buffer(yuva_frame, 0);
          if (ret<0){
              printf("Error: fill new logo frame failed:%d \n",ret);

          }


 struct SwsContext *sws_ctx= sws_getContext((*frame)->width,(*frame)->height,AV_PIX_FMT_RGBA,(*frame)->width , (*frame)->height,AV_PIX_FMT_YUVA420P,SWS_BILINEAR,NULL,NULL,NULL);

          
          sws_scale(sws_ctx,(const uint8_t * const)(*frame)->data,(*frame)->linesize,0,(*frame)->height,yuva_frame->data,yuva_frame->linesize);
          sws_freeContext(sws_ctx);

          av_frame_free(frame);


          *frame=yuva_frame;


         
  
          


}


/*  all_subtitle_logo_native_video_codec_func
 * 支持ass,srt和位图字幕 
 *
 * AVPacket *pkt,
 * AVPacket *out_pkt,
 * AVFrame *frame,
 * AVCodecContext *dec_ctx,
 * AVCodecContext **enc_ctx,
 * AVFormatContext *fmt_ctx,
 * AVFormatContext *ofmt_ctx,
 * int out_stream_index,
 * int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *,OutputStream **),
 * int *stream_mapping,
 * AVFilterGraph **filter_graph,
 * AVFilterContext **mainsrc_ctx,
 * AVFilterContext **subtitle_ctx,
 * AVFilterContext **logo_ctx,
 * AVFilterContext **resultsink_ctx,
 * FilterGraph *filter_graph_des,
 * OutputStream **output_streams  
 *
 * 
 **/

int all_subtitle_logo_native_cuda_video_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **input_streams,int *stream_mapping,AVFilterGraph **filter_graph,AVFilterContext **mainsrc_ctx,AVFilterContext **logo_ctx,AVFilterContext **subtitle_ctx,AVFilterContext **resultsink_ctx,FilterGraph *filter_graph_des,OutputStream **output_streams ){
    if(pkt!=NULL){
  //      AVFrame *sw_frame =NULL, *hw_frame=NULL; //;
        int ret,s_pts,frame_key,frame_pict_type;
        int64_t s_frame_pts;
        //uint8_t *sd=av_packet_get_side_data(pkt, AV_PKT_DATA_QUALITY_STATS,NULL);
 //sw_frame=av_frame_alloc();

 //sw_frame->height=frame->height/2;
 //sw_frame->width=frame->width/2;

        ret = avcodec_send_packet(dec_ctx, pkt);
  //av_packet_unref(pkt);
        int64_t count=0;
        if (ret < 0) {
            fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
            return 1;
        } 

        while (ret >= 0) {
            ret = avcodec_receive_frame(dec_ctx, frame);
//printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& \n");
            if (ret == AVERROR(EAGAIN)) {
        
                av_frame_unref(frame);
                av_packet_unref(pkt);
                              return ret;

            }else if( ret == AVERROR_EOF){
                return 0;
            }else if (ret < 0) {
                av_log(NULL, "Error during decoding %d \n ",ret);
                av_frame_unref(frame);
                return 1 ;
            }


            s_pts=0;
            s_frame_pts=frame->pts;

            frame_key=frame->key_frame;

            frame_pict_type=frame->pict_type;
          
            /*if (filter_graph_des->if_hw &&frame->format == AV_PIX_FMT_CUDA) {
         
                sw_frame=av_frame_alloc();
                  // retrieve data from GPU to CPU 
                if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                    av_log(NULL, "Error transferring the data to system memory\n",NULL);
                }
                av_frame_unref(frame) ;
           }else{
               sw_frame=frame;
           } 
           sw_frame->pts=s_pts;*/
//sw_frame=frame;

   /* send the frame to the encoder */ 

          // if (sw_frame!=NULL)
        //printf("Send frame %"PRId64" %"PRId64" size %"PRId64" aspect{%d %d} time_base{%d %d}\n", s_pts,filter_graph_des->current_frame_pts,sw_frame->linesize,sw_frame->sample_aspect_ratio.den,sw_frame->sample_aspect_ratio.num,dec_ctx->time_base.num,dec_ctx->time_base.den);
   //处理视频滤镜 
    //这两个变量在本文里没有用的，只是要传进去。
           AVFilterInOut *inputs, *cur, *outputs;
           AVRational logo_tb = {0};
           AVRational logo_fr = {0};

           AVFrame *logo_frame;//,*subtitle_empty_frame; 
AVBufferSrcParameters *main_buffersrc_par,*subtitle_buffersrc_par;

           logo_frame=*(filter_graph_des->logo_frame);

           //subtitle_empty_frame=*(filter_graph_des->subtitle_frame);

         
           //subtitle_empty_frame->pts=pkt->pts;

           if(  *filter_graph==NULL ){
               AVFilterGraph *filter_graph_point;

               AVFilterContext *mainsrc_ctx_point,*subtitle_ctx_point,*logo_ctx_point,*resultsink_ctx_point ;
               //初始化滤镜容器
               filter_graph_point = avfilter_graph_alloc();

               *filter_graph=filter_graph_point;
               if (!filter_graph) {
                   printf("Error: allocate filter graph failed\n");
                   return -1;
               }

               filter_graph_point->nb_threads=8;
               filter_graph_point->thread_type=FF_THREAD_FRAME;

                    // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                                  //time_base 必须与输入的dec_ctx的time一致，字幕才能正常显示
               AVRational tb = dec_ctx->time_base;
               AVRational fr = dec_ctx->framerate;//av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[0], NULL);
                    //AVRational logo_sar = logo_frame->sample_aspect_ratio;
               AVRational sar = frame->sample_aspect_ratio;

               AVBPrint args;
               av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);

               av_bprintf(&args,
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d,scale_cuda=%d:%d:format=yuv420p[main];"
                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d,hwupload_cuda[logo];"

                               "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d,hwupload_cuda[subtitle];"

                               "[main][logo]overlay_cuda=x=W-w:y=H-h-%d[main_logo];"
                               "[main_logo][subtitle]overlay_cuda=x=(W-w)/2:y=H-h-%d[result];"

                               //"[result]format=yuv420p[result_2];" 
                               "[result]scale_cuda=format=p010[result_2];"
                               "[result_2]buffersink",
                               frame->width, frame->height, frame->format, tb.num,tb.den,sar.num, sar.den,fr.num, fr.den,  frame->width,frame->height
                               ,logo_frame->width,logo_frame->height,AV_PIX_FMT_RGBA,tb.num,tb.den,sar.num, sar.den,fr.num,fr.den,
frame->width, frame->height,AV_PIX_FMT_YUVA420P, tb.num,tb.den,sar.num, sar.den,fr.num, fr.den,ENCODE_ALIGN_BITS,ENCODE_ALIGN_BITS
                               //frame->width-logo_frame->width,frame->height-logo_frame->height
                                                          //,1920,808
                 );

               ret = avfilter_graph_parse2(filter_graph_point, args.str, &inputs, &outputs);
               if (ret < 0) {
                   printf("Cannot parse2 graph\n");
                   return ret;
               }


              // hw_device_setup_for_filter(filter_graph_point, input_streams[0]->dec_ctx->hw_device_ctx);
for(int i=0;i<filter_graph_point->nb_filters;i++){
    filter_graph_point->filters[i]->hw_device_ctx=av_buffer_ref(dec_ctx->hw_device_ctx);



}
            
  main_buffersrc_par=av_buffersrc_parameters_alloc();
  memset(main_buffersrc_par,0,sizeof(*main_buffersrc_par));

main_buffersrc_par->format=AV_PIX_FMT_NONE;
main_buffersrc_par->hw_frames_ctx=frame->hw_frames_ctx;
  //根据 名字 找到 AVFilterContext
               mainsrc_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_0");



               av_buffersrc_parameters_set(mainsrc_ctx_point,main_buffersrc_par);

               *mainsrc_ctx=mainsrc_ctx_point;

 //AVFilterContext *scale_ctx_point=avfilter_graph_get_filter(filter_graph_point, "Parsed_scale_npp_1");

   //av_buffersrc_parameters_set(scale_ctx_point,par);

               logo_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_2");
 //av_buffersrc_parameters_set(logo_ctx_point,par);

               *logo_ctx=logo_ctx_point;

 subtitle_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffer_4");
 //av_buffersrc_parameters_set(logo_ctx_point,par);

               *subtitle_ctx=subtitle_ctx_point;
               resultsink_ctx_point = avfilter_graph_get_filter(filter_graph_point, "Parsed_buffersink_9");
           
               *resultsink_ctx=resultsink_ctx_point;
//printf("@@@@@@@@@@@@@@@@@@@@@@@233 %d %d \n", par->hw_frames_ctx);
   //正式打开滤镜
               ret = avfilter_graph_config(filter_graph_point, fmt_ctx);

               if (ret < 0) {
                   printf("Cannot configure graph\n");
                   return ret;
               }


               avfilter_inout_free(&inputs);
               avfilter_inout_free(&outputs);


/*处理ass,srt 字幕
 *
 * 初始化AssContext*/
   
               if (filter_graph_des->subtitle_path!=NULL&&strcmp(filter_graph_des->subtitle_path,"")>0&&filter_graph_des->ass==NULL){

                filter_graph_des->ass = (AssContext *)av_malloc(sizeof(AssContext));
                   filter_graph_des->ass->filename=filter_graph_des->subtitle_path;
                   filter_graph_des->ass->stream_index=-1;
                   filter_graph_des->ass->charenc=filter_graph_des->subtitle_charenc;
                   filter_graph_des->ass->force_style=NULL;
                   filter_graph_des->ass->fontsdir=NULL;
                   filter_graph_des->ass->original_w=0;
                   filter_graph_des->ass->alpha=0;
                   filter_graph_des->ass->renderer=NULL;


    int ret = init(filter_graph_des->ass);

    if (ret < 0)
        return ret;

   config_input(filter_graph_des->ass, AV_PIX_FMT_RGBA,  (*enc_ctx)->width,(*enc_ctx)->height);

                   if(endWith(filter_graph_des->ass->filename,LAOFMC_SUB_FILE_ASS_SUFFIX)){
//printf("@@@@@@@@@@@@@@@@@@@@@@@234 %d %d \n",12,23);
                       init_ass(filter_graph_des->ass);

                    
                   }else{
                       init_subtitles(filter_graph_des->ass);

                   }


                

               }
       


           
                }



/*完成滤镜初始化*/

/*打开enc_ctx*/

           if(!output_streams[0]->initialized){


 /*打开编码Context
            */

(*enc_ctx)->hw_device_ctx=dec_ctx->hw_device_ctx;

(*enc_ctx)->hw_frames_ctx=frame->hw_frames_ctx;

av_dict_set(&output_streams[0]->encoder_opts,"preset","fast",0);
            if ((ret = avcodec_open2(*enc_ctx, (*enc_ctx)->codec,&output_streams[0]->encoder_opts )) < 0) {
  
               av_log(NULL,AV_LOG_ERROR,"open codec faile %d \n",ret);
                          }  
//printf("@@@@@@@@@@@@@@@@@@@@@@@234 %d %d \n",(*enc_ctx)->width,(*enc_ctx)->height);

            //从编码Context拷贝参数给输出流,必须在avcodec_open2之后再设置输出流参数;
            ret = avcodec_parameters_from_context(ofmt_ctx->streams[0]->codecpar,*enc_ctx );
            if(ret < 0){
                av_log(NULL,AV_LOG_ERROR, "Failed to copy codec parameters\n");
                          }
//printf("@@@@@@@@@@@@@@@@@@@@@@@235 %d %d \n",ofmt_ctx->streams[0]->codecpar->width,ofmt_ctx->streams[0]->codecpar->height);
/*AVOutputFormat *ofmt=ofmt_ctx->oformat;
 if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, "rtsp://127.0.0.1:5544/live/test", AVIO_FLAG_WRITE);
        if (ret < 0) {
                   }
    }
    */
 /*设置RTSP视频推流协议为TCP
     */ 
    AVDictionary* options = NULL;

    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "buffer_size", "8388600", 0);

    //写入流头部信息
    ret = avformat_write_header(ofmt_ctx, &options);
    if (ret < 0) {
        av_log(ofmt_ctx,AV_LOG_ERROR, "Error occurred when opening output file\n");
        }

 
           output_streams[0]->initialized=1;

           }


             /**
              * 根据overlay的叠加顺序，必须上层先追加frame，再追加下层,如:logo_ctx必须先add，之后才能add mainsrc_ctx
              * 如果帧要重复使用，则add的必须选用AV_BUFFERSRC_FLAG_KEEP_REF这个选项，确保frame被复制之后再add，且add之后不清空frame数据
              * 如果图片要转换成视频流，则必须设置frame的pts
              *
              *
              */                   

                AVFrame *new_logo_frame;
                new_logo_frame=av_frame_alloc();

                

                new_logo_frame->width=logo_frame->width;
                new_logo_frame->height=logo_frame->height;
                new_logo_frame->format=logo_frame->format;
              
                ret=av_frame_get_buffer(new_logo_frame, 0);
                if (ret<0){
                     printf("Error: fill new logo frame failed:%d \n",ret);

                }
                ret=av_frame_copy_props(new_logo_frame, logo_frame);
                ret=av_frame_copy(new_logo_frame, logo_frame);
                if (ret<0){
                    printf("Error: copy logo frame failed:%d \n",ret);

                }
                new_logo_frame->pts=s_frame_pts;//frame的pts为0,需要设置为frame的pts
                if (filter_graph_des->if_fade ){

                     handle_logo_fade(new_logo_frame,filter_graph_des->duration_frames,filter_graph_des->interval_frames,filter_graph_des->present_frames);


                 }


//处理text字幕
                   // if (filter_graph_des->subtitle_path!=NULL&&strcmp(filter_graph_des->subtitle_path,"")>0&&filter_graph_des->ass!=NULL){
                        //frame->pts=s_frame_pts;//+av_rescale_q(fmt_ctx->start_time,AV_TIME_BASE_Q,fmt_ctx->streams[pkt->stream_index]->time_base);
                       // printf("subtite frame pts:%s %s \n", av_ts2str(s_frame_pts),av_ts2str(av_rescale_q(filter_graph_des->subtitle_time_offset*AV_TIME_BASE,AV_TIME_BASE_Q,fmt_ctx->streams[pkt->stream_index]->time_base)));
                       //
             
                       //AVFrame **subtitle_frame=(AVFrame **)av_malloc(sizeof(AVFrame **));
//memset(subtitle_frame,0,sizeof(**subtitle_frame));
                       bool if_empty_subtitle=*(filter_graph_des->subtitle_frame)==*(filter_graph_des->subtitle_empty_frame)?true:false;
                      //subtitle_frame->pts=s_frame_pts; 
                       handle_subtitle2(filter_graph_des->subtitle_frame,s_frame_pts,filter_graph_des->ass, dec_ctx->time_base,&if_empty_subtitle);
  printf("set empty %d \n",*(filter_graph_des->subtitle_frame)) ;
                       if(if_empty_subtitle){

                       
                         //printf("empty subtitle");
                /*AVFrame *new_empty_frame;
                new_empty_frame=av_frame_alloc();

                

                new_empty_frame->width=(*(filter_graph_des->subtitle_empty_frame))->width;
                new_empty_frame->height=(*(filter_graph_des->subtitle_empty_frame))->height;
                new_empty_frame->format=(*(filter_graph_des->subtitle_empty_frame))->format;
              
                ret=av_frame_get_buffer(new_empty_frame, 0);
                if (ret<0){
                     printf("Error: fill new logo frame failed:%d \n",ret);

                }
                ret=av_frame_copy_props(new_empty_frame, (*(filter_graph_des->subtitle_empty_frame)));
                ret=av_frame_copy(new_empty_frame, (*(filter_graph_des->subtitle_empty_frame)));
*/
                         *(filter_graph_des->subtitle_frame)=*(filter_graph_des->subtitle_empty_frame);
                        // if_empty_subtitle=true;
  //printf("set2 empty %d %d %d %d \n",*(filter_graph_des->subtitle_frame),filter_graph_des->subtitle_frame,*(filter_graph_des->subtitle_empty_frame),filter_graph_des->subtitle_empty_frame) ;

                       }else{

                       }
   
(*(filter_graph_des->subtitle_frame))->pts=s_frame_pts;
//int64_t subtitle_empty_frame_pts=0;
                       /* if(filter_graph_des->subtitle_time_offset>0){ 
                            subtitle_empty_frame->pts=s_frame_pts+av_rescale_q(filter_graph_des->subtitle_time_offset*1000,AV_TIME_BASE_Q,dec_ctx->time_base);
                        }else if (filter_graph_des->subtitle_time_offset<0){
                            subtitle_empty_frame->pts=s_frame_pts-av_rescale_q(-filter_graph_des->subtitle_time_offset*1000,AV_TIME_BASE_Q,dec_ctx->time_base);
                        }else {
                            subtitle_empty_frame->pts=s_frame_pts;

                        }*/
                        //subtitle_empty_frame=handle_subtitle2(subtitle_empty_frame_pts, filter_graph_des->ass, dec_ctx->time_base);
                                               //break;
                  //  }


//if(count++%2==1){
   //             subtitle_buffersrc_par->width=50;
    //            subtitle_buffersrc_par->height=50;
 //              }else{
//printf("subtitle2 x:%d y:%d\n",(*(filter_graph_des->subtitle_frame))->width,(*(filter_graph_des->subtitle_frame))->height);
 subtitle_buffersrc_par=av_buffersrc_parameters_alloc();
    //memset(subtitle_buffersrc_par,0,sizeof(*subtitle_buffersrc_par));
                subtitle_buffersrc_par->width=(*(filter_graph_des->subtitle_frame))->width;
                subtitle_buffersrc_par->height=(*(filter_graph_des->subtitle_frame))->height;
                subtitle_buffersrc_par->format=AV_PIX_FMT_YUVA420P;

  //              }
  //

   
    av_buffersrc_parameters_set(*subtitle_ctx,subtitle_buffersrc_par);
printf("b add %d %d \n",(*(filter_graph_des->subtitle_frame)),filter_graph_des->subtitle_frame);
                 ret = av_buffersrc_add_frame_flags(*subtitle_ctx, (*(filter_graph_des->subtitle_frame)),AV_BUFFERSRC_FLAG_KEEP_REF);

                 if(ret < 0){
                     av_log(NULL,"Error: av_buffersrc_add_frame failed\n",NULL);
                     av_frame_unref(frame);
                    /* if(!if_empty_subtitle){
                  av_frame_unref((*subtitle_frame));
                     av_frame_free(subtitle_frame);

}*/
                     logo_frame=NULL;
                     av_packet_unref(pkt);
                     av_packet_unref(out_pkt);
       
                     //av_frame_unref(sw_frame);
                     return ret;
                 }
//if(if_empty_subtitle){
                  //av_frame_unref((*subtitle_frame));
                  //   av_frame_free(subtitle_frame);
                  //
                  //
 //                 *(filter_graph_des->subtitle_frame)=NULL;

//}

                 //new_logo_frame->pts=s_pts;
                 ret = av_buffersrc_add_frame(*logo_ctx, new_logo_frame);

                 if(ret < 0){
                     av_log(NULL,"Error: av_buffersrc_add_frame failed\n",NULL);
                     av_frame_unref(frame);
                     av_frame_unref(new_logo_frame);
                     av_frame_free(&new_logo_frame);
                     logo_frame=NULL;
                     av_packet_unref(pkt);
                     av_packet_unref(out_pkt);
       
                     //av_frame_unref(sw_frame);
                     return ret;
                 }
                 av_frame_unref(new_logo_frame);
                 av_frame_free(&new_logo_frame);
                 //logo_frame=NULL;
   //av_frame_unref(subtitle_empty_frame);
   //              av_frame_free(&subtitle_empty_frame);

                //处理主流
                 ret = av_buffersrc_add_frame_flags(*mainsrc_ctx, frame,AV_BUFFERSRC_FLAG_PUSH);

                 if(ret < 0){
                     av_frame_unref(frame);
                     //av_frame_unref(sw_frame);
                     av_packet_unref(pkt);
                     av_packet_unref(out_pkt);
                     av_log(NULL,"Error: av_buffersrc_add_frame failed\n",NULL);
                     return ret;
                 }
//printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&3 \n");
av_frame_unref(frame);
printf("a add %d %d \n",(*(filter_graph_des->subtitle_frame)),filter_graph_des->subtitle_frame);
//free(subtitle_buffersrc_par);

                while(1){
                    ret = av_buffersink_get_frame(*resultsink_ctx, frame);
printf("a2 add %d %d \n",(*(filter_graph_des->subtitle_frame)),filter_graph_des->subtitle_frame);
                    if( ret == AVERROR_EOF ){
                    //没有更多的 AVFrame
                        av_log(NULL,"no more avframe output \n",NULL);
                        break;
                    }else if( ret == AVERROR(EAGAIN) ){
                    //需要输入更多的 AVFrame
                        av_log(NULL,"need more avframe input \n",NULL);
                        av_frame_unref(frame);
                        av_packet_unref(pkt);
                        av_packet_unref(out_pkt);
                        return ret;
                    }
//printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&2 \n");
            /*    //处理pgs字幕
                    if(filter_graph_des->sub_frame){
                        frame->pts=av_rescale_q(pkt->pts,(*enc_ctx)->time_base,fmt_ctx->streams[pkt->stream_index]->time_base);
                        handle_sub(frame, filter_graph_des->ass,filter_graph_des->sub_frame,filter_graph_des->overlay_ctx, dec_ctx->time_base) ;
                        break;
                    }else if(filter_graph_des->subtitle_stream_index>=0){
                        frame->pts=s_frame_pts;
                        handle_subtitle(frame, filter_graph_des->ass, dec_ctx->time_base) ;
                        break;
                    }
*/


                    break; 
                }

                int ret_enc;
                /*if (filter_graph_des->if_hw){
                    if (!(hw_frame = av_frame_alloc())) {
                    }
                    if ((ret = av_hwframe_get_buffer((*enc_ctx)->hw_frames_ctx, hw_frame, 0)) < 0) {
                        av_log(NULL, "Error code: %s.\n", ret);
                    }
                    if ((ret = av_hwframe_transfer_data(hw_frame, frame, 0)) < 0) {
                        av_log(NULL, "Error while transferring frame data to surface. Error code: %s.\n", ret);
                    }
    //hw_frame->pict_type=AV_PICTURE_TYPE_NONE;//解决specified frame type (3) at 358 is not compatible with keyframe interval问题
 
                    if(frame_key){
                        hw_frame->key_frame=1; 

                    } else{
                        hw_frame->key_frame=0; 
      
                    }

                    hw_frame->pict_type=frame_pict_type;
   
                    ret_enc = avcodec_send_frame(*enc_ctx, hw_frame);
                }else{
*/

               //     frame->pts=s_frame_pts;

                //    frame->pict_type=frame_pict_type;
                 //   frame->key_frame=frame_key;

         
                   ret_enc = avcodec_send_frame(*enc_ctx, frame);
av_frame_unref(frame);
             //   } 
                if (ret_enc < 0) {
                    av_frame_unref(frame);
                    //av_frame_unref(sw_frame);
                    //av_frame_free(&sw_frame);
                    av_log(NULL,AV_LOG_ERROR, "Error sending a frame for encoding %d \n",ret_enc);
                    continue;
                }
                //if (filter_graph_des->if_hw){
                 //   av_frame_unref(hw_frame);
                //}



                while (ret_enc >= 0) {
                     ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);

                     if (ret_enc == AVERROR(EAGAIN)) {
//printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&4 %d \n",ret_enc);
                         av_frame_unref(frame);
                         if (filter_graph_des->if_hw){
                 //            av_frame_free(&sw_frame);
                         } 
                         break;

                     } else if( ret_enc == AVERROR_EOF){
                  //       av_frame_unref(sw_frame);
                   //      av_frame_free(&sw_frame);
                         break;
                     } else if (ret_enc < 0) {
                         av_frame_unref(frame);
                    //     av_frame_free(&sw_frame);
                         av_log(NULL,AV_LOG_ERROR, "Error during encoding\n",NULL);
                         break; 
                     }

  //printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&2 \n");
                     if (out_pkt->data!=NULL) {

                         //out_pkt->pts=pkt->pts;
                         //out_pkt->dts=pkt->dts;
                         out_pkt->pts=av_gettime_relative()-input_streams[pkt->stream_index]->start_time;
                         out_pkt->dts=out_pkt->pts;
                         out_pkt->duration=pkt->duration;

                     }
    //输出流
                     ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,input_streams,stream_mapping,output_streams);
                     if(ret_enc<0){
                       //printf("$$$$$$$$$$$$$$$$$$$$$$$$$\n");
                         av_log(NULL,AV_LOG_ERROR,"interleaved write error:%d\n",ret_enc);
                     }
exit;
                     break;

                }
                //if (filter_graph_des->if_hw){
                 //   av_frame_free(&hw_frame);
                //}


    
          }

         return 0;
     }else{
         return 1;
     } 

}
/*只使用音频滤镜对不同音频格式进行转换,与不带only的音频滤镜不同的地方就是不预先根据frame_size的大小,对frame进行裁剪,而是通过buffersink设置frame_size自动处理。
 *
 */
uint64_t auto_audio_cuda_codec_func(AVPacket *pkt,AVPacket *out_pkt,AVFrame *frame,AVCodecContext *dec_ctx,AVCodecContext **enc_ctx,AVFormatContext *fmt_ctx,AVFormatContext *ofmt_ctx,int out_stream_index,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **input_streams,int *stream_mapping, uint64_t s_pts,OutputStream **output_streams,AVFilterContext **buffersink_ctx, AVFilterContext **buffersrc_ctx ,AVFilterGraph **filter_graph,char *afilter_desrc){

  if(pkt!=NULL){


    int ret;

     uint64_t origin_pkt_pts=pkt->pts;
      uint64_t origin_pkt_duration=pkt->duration;
  printf("dec time base:{%d %d}input pkt duration:%"PRId64"\n",dec_ctx->time_base.den,dec_ctx->time_base.num,pkt->duration);
 printf(" input pkt duration:%"PRId64"\n",pkt->duration);


    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
 av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        //av_frame_unref(frame);

        fprintf(stderr, "Error sending a packet for decoding %d \n",ret);
        return 1;
    }


    while (ret >= 0) {


        ret = avcodec_receive_frame(dec_ctx, frame);
 //printf("in sample fmt:%d\n",frame->format);
      if (ret == AVERROR(EAGAIN)) {
 av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_frame_unref(frame);

        break;
      }else if( ret == AVERROR_EOF){
 av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_frame_unref(frame);
                 return 0;
      }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding %d \n ",ret);
 av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_frame_unref(frame);

            return 1 ;
        }
    int enc_frame_size=(*enc_ctx)->frame_size;
           float factor=(float)enc_frame_size/(float)frame->nb_samples;
           int frame_size=frame->nb_samples;
      int s_stream_index=pkt->stream_index;
      int s_duation=origin_pkt_duration*factor;
      uint64_t origin_s_pts=s_pts; 
        //}
  //bool if_recreate_pts=true;


   /*     if(enc_frame_size>frame->nb_samples&&enc_frame_size%frame->nb_samples==0) {
          if_recreate_pts=false;
        }
*/
        //AVFrame *frame_enc;
    if (frame!=NULL){



       // printf("Send frame audio :%"PRId64" size %"PRId64" stream index%d aspect{%d %d} factor:%f \n", frame->pts,frame->linesize,pkt->stream_index,frame->sample_aspect_ratio.den,frame->sample_aspect_ratio.num,factor);
    //int enc_frame_size=(*enc_ctx)->frame_size;
          //音频重采集
       if(*filter_graph==NULL){

         if(init_audio_filters(afilter_desrc, buffersink_ctx, buffersrc_ctx, filter_graph, dec_ctx, dec_ctx->time_base,(*enc_ctx))){

         }

       }
   if (av_buffersrc_add_frame_flags(*buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                        av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                                         }
s_pts+=frame->nb_samples;

//printf("s_pts w:%"PRId64"\n" ,s_pts);


 av_frame_unref(frame);

                    /* pull filtered audio from the filtergraph */
                    while (ret >=0) {
                        ret = av_buffersink_get_frame(*buffersink_ctx, frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
 av_packet_unref(pkt);
 //
        av_packet_unref(out_pkt);
                          //s_pts=s_pts+pkt->duration;
                           //return s_pts; 
                           break;
                        }
                        if (ret < 0){
                           break;
                        }

                                               s_pts-=enc_frame_size;

//printf("s_pts r:%"PRId64"\n" ,s_pts);
        int ret_enc = avcodec_send_frame(*enc_ctx, frame);
    if (ret_enc < 0) {
        fprintf(stderr, "Error sending a frame for encoding %d \n",ret_enc);
       return 1; 
    } 
      //接收编码完成的packet
    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(*enc_ctx, out_pkt);
          if (ret_enc == AVERROR(EAGAIN)) {
 av_packet_unref(pkt);
 //
        av_packet_unref(out_pkt);
                          av_frame_unref(frame);
 //s_pts=s_pts+pkt->duration;

                    break;
         }else if( ret_enc == AVERROR_EOF){
 av_packet_unref(pkt);
        av_packet_unref(out_pkt);
                      av_frame_unref(frame);

            break;

    
      }else if (ret_enc < 0) {
            fprintf(stderr, "Error during encoding\n");
                         av_frame_unref(frame);
            break;
        }
              
 av_frame_unref(frame);
 if (out_pkt->data!=NULL) {
 
pkt->stream_index=s_stream_index;
//out_pkt->pts=pkt->pts;
//out_pkt->

// out_pkt->pts=pkt->pts;
//if (pkt->duration==0){

      //printf("truehd pts:%"PRId64" s_pts :%"PRIu64" duration :%"PRIu64"\n",pkt->pts,s_pts,pkt->duration);
//
     // out_pkt->pts=pkt->pts;

//out_pkt->pts=av_gettime_relative()-input_streams[stream_mapping[pkt->stream_index]]->start_time+AV_TIME_BASE*0.08;
out_pkt->pts=av_gettime_relative()-input_streams[stream_mapping[pkt->stream_index]]->start_time;
 //printf("####################3\n");
      out_pkt->duration=pkt->duration;
    
 //   } else if(if_recreate_pts){
  //  out_pkt->pts=s_pts+out_pkt->duration;
   // }else {
    //out_pkt->pts=pkt->pts-pkt->duration*(factor-1);
//}
//if((out_pkt->pts % 3) == 0){
 //  out_pkt->pts++;
//}
//
//
    if (factor<1&factor>0){
//
     out_pkt->duration=s_duation;
  /*  //out_pkt->pts=s_pts;
   if(origin_s_pts==0){

      out_pkt->pts=origin_pkt_pts;

    }else{
     // out_pkt->pts=pkt->pts-pkt->duration*(factor-1);a
     //
     float leve_factor=(float)s_pts/(float)frame_size;

     printf(" leve fatch:%.2f\n",leve_factor);
     //uint64_t intver_duration=av_rescale_q(origin_s_pts/,AV_TIME_BASE_Q, (*enc_ctx)->time_base);
     out_pkt->pts=origin_pkt_pts-origin_pkt_duration+origin_pkt_duration*(1-leve_factor);


printf("origin_s_pts:%"PRId64" out pts:%"PRId64" in pts:%"PRId64" \n",origin_s_pts,out_pkt->pts,pkt->pts);
    }
*/
}
//output_streams[stream_mapping[pkt->stream_index]]->frame_number++;
 out_pkt->dts=out_pkt->pts;
 //s_pts = out_pkt->pts;

 //s_pts = out_pkt->pts+out_pkt->duration;
 }else{

 }
//out_pkt->dts=out_pkt->pts;
  if(!output_streams[0]->initialized){
 av_frame_unref(frame);
 av_packet_unref(pkt);
        av_packet_unref(out_pkt);
return s_pts;
  }


     // av_packet_rescale_ts(out_pkt,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base,input_streams[stream_mapping[pkt->stream_index]]->st->time_base);
    //输出流

    ret_enc=(*handle_interleaved_write_frame)(pkt,out_pkt,frame,dec_ctx,enc_ctx,fmt_ctx,ofmt_ctx,input_streams,stream_mapping,output_streams);


   
    av_frame_unref(frame);
    //av_frame_free(&frame_enc);

    if(ret_enc<0){
        printf("interleaved write error:%d",ret_enc);
//av_packet_unref(pkt);//确保非输出packet也释放
        //return ret_enc;
    }
//if (factor<1&factor>0){


 //   s_pts=s_pts+s_duation;


    /*if((output_streams[stream_mapping[pkt->stream_index]]->frame_number%(int)(1/(1-factor)))==0){
        out_pkt->pts++;

    }*/
//}//


    av_packet_unref(out_pkt);
   
    }


      }
    //return s_pts;  

    }
  
 
    }
}
  

return s_pts;

}


/*  push2trtsp_sub_logo
 * 将视频流通过rtsp协议推送到直播服务器，硬编字幕,并实现logo 的周期淡入淡出
 * 
 *
 * video_file_path:
 * video_index:
 * audio_index:
 * subtitle_index;
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

int push2rtsp_sub_logo_cuda(const char *video_file_path, const int video_index, const int audio_index,const int subtitle_index,const char *subtitle_file_path,AVFrame **logo_frame,const char *rtsp_push_path,bool if_hw,bool if_logo_fade,uint64_t duration_frames,uint64_t interval_frames,uint64_t present_frames,TaskHandleProcessInfo *task_handle_process_info){
    
    /*函数内全局输入输出流数组&流数目
     */
    InputStream **input_streams = NULL;
    int        nb_input_streams = 0;
    OutputStream **output_streams = NULL;
    int        nb_output_streams = 0;

    AVOutputFormat *ofmt = NULL;

    SubtitleFrame *sub_frame=NULL;

    /*初始化输入ForamtContext
     */
    AVFormatContext *ifmt_ctx = avformat_alloc_context(), *ofmt_ctx = NULL;
    AVBufferRef **hw_device_ctx =(AVBufferRef **)av_malloc(sizeof(AVBufferRef **));


    /*初始化pkt,out_pkt,frame*/
    AVPacket *pkt=av_packet_alloc();
    AVPacket *out_pkt=av_packet_alloc();
    AVFrame *frame=av_frame_alloc();


    /*初始化视频滤镜*/

    AVFilterGraph **filter_graph= (AVFilterGraph **)av_malloc(sizeof(AVFilterGraph *));
    *filter_graph=NULL;
    //主源
    AVFilterContext **mainsrc_ctx=(AVFilterContext **)av_malloc(sizeof(AVFilterContext **));
    //logo源 
    AVFilterContext **logo_ctx=(AVFilterContext **)av_malloc(sizeof(AVFilterContext **));
    AVFilterContext **subtitle_ctx=(AVFilterContext **)av_malloc(sizeof(AVFilterContext **));

    AVFilterContext **resultsink_ctx = (AVFilterContext **)av_malloc(sizeof(AVFilterContext **));
   
   /*初始化滤镜描述*/
    FilterGraph *filter_graph_des = av_mallocz(sizeof(*filter_graph_des));
    filter_graph_des->subtitle_path=av_mallocz(strlen(subtitle_file_path)+1);
    
    filter_graph_des->current_frame_pts=0;
    filter_graph_des->ass=NULL;
    filter_graph_des->overlay_ctx=NULL;

    /*复制字幕文件名*/
    strcpy(filter_graph_des->subtitle_path, subtitle_file_path);


    /*初始化指向音频滤镜指针的指针*/
    AVFilterGraph **afilter_graph= (AVFilterGraph **)av_malloc(sizeof(AVFilterGraph **));
    *afilter_graph=NULL;

    /*初始化指向音频滤镜src指针的指针*/
    AVFilterContext **abuffersrc_ctx=(AVFilterContext **)av_malloc(sizeof(AVFilterContext **));
    *abuffersrc_ctx=NULL;
    /*初始化指向音频滤镜sink指针的指针*/
    AVFilterContext **abuffersink_ctx = (AVFilterContext **)av_malloc(sizeof(AVFilterContext **));
    *abuffersink_ctx=NULL;

    /*音频滤镜定义语句*/
    char *afilters_descr=NULL;
     /*初始化logo帧*/
    AVFrame *new_logo_frame=av_frame_alloc();

    new_logo_frame->width=(*logo_frame)->width;
    new_logo_frame->height=(*logo_frame)->height;
    new_logo_frame->format=(*logo_frame)->format;

    
    int ret;
    /*初始化logo帧的buffer空间*/               
    ret=av_frame_get_buffer(new_logo_frame, 0) ;
    if (ret<0){
        printf("Error: fill new logo frame failed:%d \n",ret);

    }

    /*拷贝输入logo帧到新的logo帧,避免输入logo帧被外部调用程序释放,如：golang*/      
    ret=av_frame_copy_props(new_logo_frame, (*logo_frame));//拷贝属性
    ret=av_frame_copy(new_logo_frame, (*logo_frame));//拷贝数据
    if (ret<0){
        printf("Error: copy logo frame failed:%d \n",ret);

    }


//AVFrame  *subtitle_empty_frame=NULL;g
 // if(!subtitle_empty_frame)en_empty_layout_frame(100,100);
 //
    filter_graph_des->subtitle_empty_frame=(AVFrame **)av_malloc(sizeof(AVFrame **));

    filter_graph_des->subtitle_frame=(AVFrame **)av_malloc(sizeof(AVFrame **));
//memset(filter_graph_des->subtitle_frame,0,sizeof(AVFrame **));
    *(filter_graph_des->subtitle_frame)=NULL;
gen_empty_layout_frame_yuva420p(filter_graph_des->subtitle_empty_frame,10,10);
    filter_graph_des->logo_frame=&new_logo_frame;
    //filter_graph_des->subtitle_frame=&subtitle_empty_frame;
  
    /*手动释放输入logo帧*/ 
    av_frame_unref(*logo_frame);
    av_frame_free(logo_frame);
    logo_frame=NULL;
    
    filter_graph_des->if_hw=if_hw;
    filter_graph_des->if_fade=if_logo_fade;

    filter_graph_des->duration_frames=duration_frames;
    filter_graph_des->interval_frames = interval_frames;
    filter_graph_des->present_frames=present_frames;

   
    //filter_graph_des->sub_type=sub_type;
    filter_graph_des->subtitle_stream_index=subtitle_index;
   


    int  i;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    bool ifcodec=true;//是否解编码
    bool rate_emu = true;//是否按照播放速度推流
    int audio_pts=0;

    /*解码Context和编码Context
     */
    AVCodecContext *dec_ctx= NULL,*enc_ctx= NULL;
    enum AVHWDeviceType type;



    /*视频解编码处理函数
      */
    int (*handle_video_codec)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *,InputStream **input_streams,OutputStream **output_streams);  

    /*视频滤镜解编码处理函数
      */
    int (*handle_video_complex_filter)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **,int *,AVFilterGraph **,AVFilterContext **,AVFilterContext **,AVFilterContext **,AVFilterContext **,FilterGraph *,OutputStream **output_streams );  



    /*音频解编码处理函数
     * */
    uint64_t (*handle_audio_codec)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int *),int *,uint64_t); 

    /*音频滤镜解编码处理函数
      */
    uint64_t (*handle_audio_complex_filter)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,int,int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **),InputStream **,int *,uint64_t,OutputStream **,AVFilterContext **, AVFilterContext **,AVFilterGraph **,char *afilter_desrc);

    /*字幕解编码处理函数
      */
    int (*handle_subtitle_codec)(AVPacket *pkt,SubtitleFrame *subtitle_frame,AVCodecContext *sub_condec_ctx,AVFormatContext *fmt_ctx,int *stream_mapping,InputStream **input_streams,OutputStream **output_streams,AssContext *ass);  


    /*帧输出处理函数
     * */
    int (*handle_interleaved_write_frame)(AVPacket *,AVPacket *,AVFrame *,AVCodecContext *,AVCodecContext **,AVFormatContext *,AVFormatContext *,InputStream **,int *,OutputStream **);  

   

    /*设置日志级别
     * */


    av_log_set_level(AV_LOG_DEBUG);
   
    
    /*设置帧处理函数
     */ 
    handle_video_codec=simple_video_codec_func;
    handle_interleaved_write_frame=seek_interleaved_write_frame_func;
    handle_video_complex_filter=all_subtitle_logo_native_cuda_video_codec_func;
    handle_audio_complex_filter=auto_audio_cuda_codec_func;
    handle_subtitle_codec=simple_subtitle_codec_func;

    /*设置ifmt的callback处理函数
     */
    ifmt_ctx->interrupt_callback=int_cb;

    
    /*初始化硬件加速设备
     */ 
    if(if_hw){
      type = av_hwdevice_find_type_by_name("cuda");
      if (type == AV_HWDEVICE_TYPE_NONE) {
          av_log(NULL,AV_LOG_ERROR, "Device type %s is not supported.\n", "cuda");
          av_log(NULL,AV_LOG_ERROR, "Available device types:");
          while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
              av_log(NULL,AV_LOG_ERROR, " %s \n", av_hwdevice_get_type_name(type));
        }
    }




    /*打开视频文件
     */ 
    if ((ret = avformat_open_input(&ifmt_ctx, video_file_path, 0, 0)) < 0) {
        av_log(NULL,AV_LOG_ERROR, "Could not open input file '%s'", video_file_path);
        goto end;
    }

    /*查看视频流信息
     */ 
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(NULL,AV_LOG_ERROR,"Failed to retrieve input stream information");
        goto end;
    }

    /*打印视频流信息
     */ 
    av_dump_format(ifmt_ctx, 0, video_file_path, 0);
// ret=handle_seek(ifmt_ctx,task_handle_process_info->control );

  //task_handle_process_info->control->subtitle_time_offset+=task_handle_process_info->control->seek_time*1000;
 //task_handle_process_info->control->seek_time=0x00;
 //ret = av_read_frame(ifmt_ctx, pkt); 

    /*打开RTSP视频推流Context
     */ 
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "RTSP", rtsp_push_path);
    if (!ofmt_ctx) {
       av_log(NULL,AV_LOG_ERROR,  "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    

    /*设置RTSP视频推流协议为TCP
     */ 
    AVDictionary* options = NULL;

    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "buffer_size", "838860000", 0);

  
    stream_mapping_size = ifmt_ctx->nb_streams;

    /*初始化stream_mapping 
     */
    stream_mapping = av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ofmt = ofmt_ctx->oformat;


    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        stream_mapping[i] = -1;

        //选择视频流,音频流和字幕流
        if (i!=video_index&&i!=audio_index){
            //&&i!=subtitle_index){
        //if(i!=video_index){
            continue;
        }
        //数组扩容
        GROW_ARRAY(input_streams, nb_input_streams);

        av_log(NULL,AV_LOG_DEBUG,"i: %d nb_input_streams %d \n",i,nb_input_streams);
               //增加输入流
        add_input_stream_cuda(ifmt_ctx,i,nb_input_streams-1,if_hw,type,hw_device_ctx,input_streams);
        

        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];

        //输入流解码参数
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        stream_mapping[i]=nb_input_streams-1;

        if(i==subtitle_index&&in_codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE){
            filter_graph_des->subtitle_dec_ctx=input_streams[nb_input_streams-1]->dec_ctx;
            AVCodecDescriptor *dec_desc = avcodec_descriptor_get(in_stream->codecpar->codec_id);
            if (dec_desc && (dec_desc->props & AV_CODEC_PROP_BITMAP_SUB)){

               sub_frame=alloc_subtitle_frame();
               sub_frame->pts=0;
               filter_graph_des->sub_frame=sub_frame;

            }
        }

        if(in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO||in_codecpar->codec_type==AVMEDIA_TYPE_VIDEO){

            //新建ofmt_ctx的输出流
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                av_log(NULL,AV_LOG_ERROR, "Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }

        
        /*进入解编码处理
         */
            
        if(ifcodec){
            GROW_ARRAY(output_streams, nb_output_streams);
            OutputStream *output_stream;

            /*配置输出流,字幕流不配置输出流
             */
            if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
                //音频,RTSP推流，音频只支持AAC
                output_stream = add_output_stream_cuda(ofmt_ctx, nb_output_streams-1,if_hw, in_codecpar->codec_type,AV_CODEC_ID_AAC,output_streams);
            }else if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO){

                //视频
                output_stream = add_output_stream_cuda(ofmt_ctx, nb_output_streams-1,if_hw, in_codecpar->codec_type,task_handle_process_info->video_codec_id,output_streams);

           }


           if (!output_stream) {
              av_log(NULL,AV_LOG_ERROR, "Could not allocate output stream\n");
              ret = AVERROR_UNKNOWN;
              goto end;
        
           }
 
            enc_ctx=output_stream->enc_ctx;

           /*设置enc_ctx的视频
            */
           if (in_codecpar->codec_type==AVMEDIA_TYPE_VIDEO){



           /*统计进度,已视频流为主
            */

              if (task_handle_process_info==NULL){

                  TaskHandleProcessInfo *info=av_malloc(sizeof(TaskHandleProcessInfo *)); 

                  task_handle_process_info=info  ; 
              }

              av_log(NULL,AV_LOG_DEBUG,"Total frames:%"PRId64"\n",ifmt_ctx->duration); 

              task_handle_process_info->total_duration=ifmt_ctx->duration;

              task_handle_process_info->pass_duration=0;

              //提取输入流的比特率
              if(task_handle_process_info->bit_rate==0){

                  task_handle_process_info->bit_rate=ifmt_ctx->bit_rate;
                    

                  enc_ctx->bit_rate=task_handle_process_info->bit_rate;
              }else if (task_handle_process_info->bit_rate>0){

                  enc_ctx->bit_rate=task_handle_process_info->bit_rate;

              }
      
              //设置编码Context的编码器类型      
              enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
              //设置编码Context的time_base 
              enc_ctx->time_base = input_streams[nb_input_streams-1]->dec_ctx->time_base;
                    
              av_log(NULL,AV_LOG_DEBUG,"video stream time base:{%d %d} bitrate :%d \n",enc_ctx->time_base.den,enc_ctx->time_base.num,enc_ctx->bit_rate);
              //设置编码Context的尺寸 

              if(task_handle_process_info->width>0&&task_handle_process_info->height>0){
                  enc_ctx->width=task_handle_process_info->width;
                  enc_ctx->height=task_handle_process_info->height; 


              }else{
                  enc_ctx->width = input_streams[nb_input_streams-1]->dec_ctx->width;
                  enc_ctx->height = input_streams[nb_input_streams-1]->dec_ctx->height;

              }

              enc_ctx->height=aligned_frame_height(enc_ctx->height);

              //enc_ctx->width=3840;
              //enc_ctx->height=1616;
                        
              enc_ctx->sample_aspect_ratio = input_streams[nb_input_streams-1]->dec_ctx->sample_aspect_ratio;


//init_filter_graph_func(pkt,out_pkt,frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,&output_streams[stream_mapping[pkt->stream_index]]->enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping[pkt->stream_index],handle_interleaved_write_frame,input_streams,stream_mapping,filter_graph,mainsrc_ctx,logo_ctx,resultsink_ctx,filter_graph_des, output_streams);
 
//printf("!!!!!!!!!!!!!!!!!!!!!11 %d \n",input_streams[nb_input_streams-1]->dec_ctx->hw_);
              if(if_hw){
              //硬件加速    
                  enc_ctx->pix_fmt=AV_PIX_FMT_CUDA;

              /* set hw_frames_ctx for encoder's AVCodecContext 
               * 设置编码Context的hwframe context
               * */

                  /*if ( set_hwframe_ctx_cuda(enc_ctx, *resultsink_ctx) < 0) {
                      av_log(NULL,AV_LOG_ERROR, "Failed to set hwframe context.\n");
                      ret = AVERROR_UNKNOWN;
                      goto end;
                  } 
*/
               }else{
                   enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

               }                                      
               if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
                  enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

               }

            /*设置enc_ctx的视频
            */
            }else if(in_codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
                enc_ctx->sample_rate=input_streams[nb_input_streams-1]->dec_ctx->sample_rate;

               /*需要通过声道数找默认的声道布局，不然会出现aac 5.1(side)
                * */

                if(task_handle_process_info->audio_channels==0){         
                    enc_ctx->channels=input_streams[nb_input_streams-1]->dec_ctx->channels;
                }else{
                    enc_ctx->channels=task_handle_process_info->audio_channels;
                }
                        
                enc_ctx->channel_layout=av_get_default_channel_layout(enc_ctx->channels);
                //enc_ctx->channel_layout=AV_CH_LAYOUT_7POINT1_WIDE;            
                enc_ctx->time_base = input_streams[nb_input_streams-1]->dec_ctx->time_base;

                av_log(NULL,AV_LOG_DEBUG,"audio stream time base:{%d %d} sample_rate:%d channel_layout:%d channels:%d\n",enc_ctx->time_base.den,enc_ctx->time_base.num,enc_ctx->sample_rate,enc_ctx->channel_layout,enc_ctx->channels);
                //enc_ctx->strict_std_compliance=-2;
                //rtsp只支持acc音频编码,acc编码的采样格式为flp
                enc_ctx->sample_fmt=AV_SAMPLE_FMT_FLTP;
                //设置音频滤镜描述 
                AVBPrint arg;
                av_bprint_init(&arg, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&arg, "aformat=sample_fmts=%s",av_get_sample_fmt_name(enc_ctx->sample_fmt));
                afilters_descr=arg.str;


            }else{
                continue;
            }

       
            if(in_codecpar->codec_type!=AVMEDIA_TYPE_VIDEO){
           /*打开编码Context
            */
            if ((ret = avcodec_open2(enc_ctx, output_stream->enc,&output_stream->encoder_opts )) < 0) {
                  av_log(NULL,AV_LOG_ERROR,"open codec faile %d \n",ret);
                 goto end;
            }  

            //从编码Context拷贝参数给输出流,必须在avcodec_open2之后再设置输出流参数;
            ret = avcodec_parameters_from_context(out_stream->codecpar,enc_ctx );
            if(ret < 0){
                av_log(NULL,AV_LOG_ERROR, "Failed to copy codec parameters\n");
                 goto end;
            }

            }
        }else{

            //不编解码仅有Packet转发
            ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
            if (ret < 0) {
                av_log(NULL,AV_LOG_ERROR, "Failed to copy codec parameters\n");
                goto end;
            }

        } 
        
    }}
    //打印输出流信息
    av_dump_format(ofmt_ctx, 0, rtsp_push_path, 1);
    //打开输出流文件
    //

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, rtsp_push_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(ofmt_ctx,AV_LOG_ERROR, "Could not open output file '%s'", rtsp_push_path);
            goto end;
        }
    }
    //写入流头部信息
    /*ret = avformat_write_header(ofmt_ctx, &options);
    if (ret < 0) {
        av_log(ofmt_ctx,AV_LOG_ERROR, "Error occurred when opening output file %d \n", ret);
        goto end;
    }*/

    /*完成编解码context设置,开始读取packet，并逐packet处理
     */

    AVStream *in_stream, *out_stream;
    InputStream *ist;

    //bool retry =true;

    int64_t start_frame=0,end_frame=0;
    int64_t paused_time=0;
// ret=handle_seek(ifmt_ctx,task_handle_process_info->control->seek_time );
//task_handle_process_info->control->seek_time=0x00;
    //循环读取packet,直到接到任务取消信号
    int count_1,count_2=0;
    while (!task_handle_process_info->control->task_cancel) {

   /*处理控制速度
         */
        
        if (rate_emu&&input_streams[0]) {
            bool if_read_packet=true;

            //for (i = 0; i < 1; i++) {

                ist = input_streams[0];
                int64_t pts=av_rescale_q(ist->pts, ist->st->time_base, AV_TIME_BASE_Q);
                int64_t current_timestap=av_gettime_relative();
                int64_t now = current_timestap-ist->start;
//printf("&&&&&&&&&&& %d %"PRId64" %"PRId64"  %"PRId64" %d \n",i,pts,now,ist->start,pts-now);
//
//

//if(i==0){
//控设置视频流缓存的时间,高帧率视频需要调大设置缓冲时间
int cache_time=0.5;//-1 - 1
  //TS设置缓冲1个AV_TIME_BASE
                if (pts > now+AV_TIME_BASE*cache_time){
                    if_read_packet=false;
 //printf("************************* count_1:%d \n",count_1);

                    //count_1++;
//                    break;
                }

                
/*}else{
// if (pts > now+(input_streams[1]->start-input_streams[0]->start)-1){
 if (pts > now){
                    if_read_packet=false;
 //printf("************************* count_2:%d  %"PRId64"\n",count_2,now+(input_streams[1]->start-input_streams[0]->start));
                    count_2++;
                    break;
                }

}*/
//};
            if (!if_read_packet){
             // printf("************************* %d %d  %f \n",count_1,count_2,0);
                             continue;
            }

count_1=count_2=0;
        };

      /*处理暂停*/
      if(task_handle_process_info->control->task_pause){

        if(!paused_time){

          paused_time=av_gettime_relative();

        }
       // sleep(1);
 for(int i=0;i<nb_output_streams;i++){
 ist = input_streams[i];

 //out_pkt=pkt;//output_streams[0]->pkt;
 out_pkt->pts=av_gettime_relative()-input_streams[i]->start_time;
                         out_pkt->dts=out_pkt->pts;
                         out_pkt->duration=pkt->duration;
                         out_pkt->stream_index=i;

     /*if (rate_emu) {
            //ist = input_streams[i];
            //if (pkt != NULL && pkt->dts != AV_NOPTS_VALUE) {
               // ist->next_dts = ist->dts += av_rescale_q(AV_TIME_BASE,  AV_TIME_BASE_Q,ist->st->time_base);
//ist->next_dts = ist->dts =out_pkt->dts;
//ist->next_pts = ist->pts =out_pkt->pts;
               // ist->next_pts = ist->pts += av_rescale_q(AV_TIME_BASE,  AV_TIME_BASE_Q,ist->st->time_base);
           // }
     //}
     int64_t pts=av_rescale_q(ist->pts, ist->st->time_base, AV_TIME_BASE_Q);

     int64_t now = av_gettime_relative()-ist->start;

     printf("&&&&&&&&&&&_new %d %"PRId64" %"PRId64"  %"PRId64" %d \n",i,pts,now,ist->start,pts-now);

 }*/
 av_packet_rescale_ts(out_pkt,AV_TIME_BASE_Q,ofmt_ctx->streams[i]->time_base);
      //直接输出packet 
     log_packet(ofmt_ctx, out_pkt, "out");

        ret = av_interleaved_write_frame(ofmt_ctx, out_pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
 //av_packet_unref(pkt);
        av_packet_unref(pkt);
        av_packet_unref(out_pkt);
       // av_frame_unref(frame);


return ret;
  //          break;
        }
//}  
        av_packet_unref(pkt);
        av_packet_unref(out_pkt);
      }
        continue;

    }else{
      if (paused_time){

          for (i = 0; i < nb_input_streams; i++) {
              ist = input_streams[i];

              ist->start+=(av_gettime_relative()-paused_time);

              ist->pts-=av_rescale_q((av_gettime_relative()-paused_time),AV_TIME_BASE_Q,input_streams[i]->st->time_base);


          }

          paused_time=0;
    }
    
    }

    /*处理前进和后退*/
      if(task_handle_process_info->control->seek_time!=0){//&&(end_frame==2000||end_frame==10000)){

        task_handle_process_info->control->current_seek_pos_time=av_rescale_q(input_streams[stream_mapping[0]]->pts, input_streams[stream_mapping[0]]->st->time_base, AV_TIME_BASE_Q);
               ret=handle_seek(ifmt_ctx,task_handle_process_info->control );
      printf("************************* %d %d  %f \n",0,1,0);
        //task_handle_process_info->control->subtitle_time_offset+=task_handle_process_info->control->current_seek_pos_time/1000;
        if(ret==0){

          av_log(ifmt_ctx, AV_LOG_INFO, "seek %"PRId64" seconds \n",task_handle_process_info->control->seek_time);
          for(int i=0;i<nb_output_streams;i++){
        //av_buffer_unref(&output_streams[i]->enc_ctx->hw_frames_ctx);
        //avcodec_close(output_streams[i]->enc_ctx);

             input_streams[i]->start-=(task_handle_process_info->control->seek_time*AV_TIME_BASE);
//input_streams[i]->start-=(task_handle_process_info->control->current_seek_pos_time+task_handle_process_info->control->seek_time*AV_TIME_BASE);
input_streams[i]->pts+=av_rescale_q(task_handle_process_info->control->seek_time*AV_TIME_BASE,AV_TIME_BASE_Q,input_streams[i]->st->time_base);
          }


          task_handle_process_info->control->seek_time=0x00;

        }

      }


             /*从输入流读取帧，保存到pkt
         * */
        ret = av_read_frame(ifmt_ctx, pkt); 

        //uint8_t *sd=av_packet_get_side_data(pkt, AV_PKT_DATA_QUALITY_STATS, NULL);
        //printf("read frame side data:%d\n",sd); 
        if (ret == AVERROR(EAGAIN)) {
            continue;
        }else if(ret == AVERROR_EOF){

            task_handle_process_info->handled_rate=1.00;
            goto end;
          
        }
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }  

        av_log(ifmt_ctx,AV_LOG_DEBUG," input pkt duration:%"PRId64"\n",pkt->duration);
  end_frame++;

        in_stream  = ifmt_ctx->streams[pkt->stream_index];

        if (pkt->stream_index >= stream_mapping_size ||
            stream_mapping[pkt->stream_index] < 0) {
            av_packet_unref(pkt);

            continue;
        }
        if(filter_graph_des->subtitle_time_offset!=task_handle_process_info->control->subtitle_time_offset){
            filter_graph_des->subtitle_time_offset=task_handle_process_info->control->subtitle_time_offset;
        }

        if(filter_graph_des->subtitle_charenc!=task_handle_process_info->control->subtitle_charenc){
            filter_graph_des->subtitle_charenc=task_handle_process_info->control->subtitle_charenc;
        }
//
//log_packet(ifmt_ctx, pkt, "in-2");
/*
         if(pkt->pts-input_streams[stream_mapping[pkt->stream_index]]->start_pts>input_streams[stream_mapping[pkt->stream_index]]->origin_stream_pts){
            pkt->pts=pkt->pts-input_streams[stream_mapping[pkt->stream_index]]->start_pts;
        }else{
            pkt->pts=pkt->pts+input_streams[stream_mapping[pkt->stream_index]]->origin_stream_pts-input_streams[stream_mapping[pkt->stream_index]]->start_pts+2*pkt->duration;
         //  

        }

       if(pkt->dts-input_streams[stream_mapping[pkt->stream_index]]->start_pts>input_streams[stream_mapping[pkt->stream_index]]->origin_stream_dts){

            pkt->dts=pkt->dts-input_streams[stream_mapping[pkt->stream_index]]->start_pts;
        }else{
            pkt->dts=pkt->dts+input_streams[stream_mapping[pkt->stream_index]]->origin_stream_dts-input_streams[stream_mapping[pkt->stream_index]]->start_pts+2*pkt->duration;
//input_streams[stream_mapping[pkt->stream_index]]->start_pts=0;

        }
//printf("strat_time:%"PRId64" %"PRId64"  \n",ifmt_ctx->start_time,input_streams[stream_mapping[pkt->stream_index]]->start_pts);
//
        input_streams[stream_mapping[pkt->stream_index]]->origin_stream_pts=pkt->pts;
        input_streams[stream_mapping[pkt->stream_index]]->origin_stream_dts=pkt->dts;*/
log_packet(ifmt_ctx, pkt, "in-2");
//printf("seek_time:%"PRId64" %"PRId64"  \n",task_handle_process_info->control->seek_time*AV_TIME_BASE,av_rescale_q(task_handle_process_info->control->seek_time*AV_TIME_BASE,AV_TIME_BASE_Q,input_streams[stream_mapping[pkt->stream_index]]->st->time_base));

      //   pkt->pts=pkt->pts-(input_streams[stream_mapping[pkt->stream_index]]->start_pts+av_rescale_q(task_handle_process_info->control->seek_time*AV_TIME_BASE,AV_TIME_BASE_Q,input_streams[stream_mapping[pkt->stream_index]]->st->time_base));
pkt->pts=pkt->pts-input_streams[stream_mapping[pkt->stream_index]]->start_pts;

if(pkt->dts==AV_NOPTS_VALUE){

  pkt->dts=pkt->pts;
  //out_pkt->dts=pkt->dts;

}else{
 //pkt->dts=pkt->dts-(input_streams[stream_mapping[pkt->stream_index]]->start_pts+av_rescale_q(task_handle_process_info->control->seek_time*AV_TIME_BASE,AV_TIME_BASE_Q,input_streams[stream_mapping[pkt->stream_index]]->st->time_base));
 pkt->dts=pkt->dts-input_streams[stream_mapping[pkt->stream_index]]->start_pts;
}


   /*处理控速
         * */
        if (rate_emu) {
            ist = input_streams[stream_mapping[pkt->stream_index]];
            if (pkt != NULL && pkt->dts != AV_NOPTS_VALUE) {
                //ist->next_dts = ist->dts = av_rescale_q(pkt->dts, in_stream->time_base, AV_TIME_BASE_Q);
ist->next_dts = ist->dts =pkt->dts;}
ist->next_pts = ist->pts =pkt->pts;
                //ist->next_pts = ist->pts = av_rescale_q(pkt->pts, in_stream->time_base, AV_TIME_BASE_Q);            }
        }

        /*处理音视频的解码和编码
         *
         */
        if (ifcodec&&(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO||ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO||ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_SUBTITLE)){

            //处理视频解编码
            if(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO&&(handle_video_codec!=NULL||handle_video_complex_filter!=NULL)){

                log_packet(ifmt_ctx, pkt, "in");
                //已消费帧计数加一
//task_handle_process_info->control->current_seek_pos_time=av_rescale_q(pkt->pts,ifmt_ctx->streams[pkt->stream_index]->time_base,AV_TIME_BASE_Q);
                //input_streams[stream_mapping[pkt->stream_index]]->current_pts=pkt->pts;
                task_handle_process_info->pass_duration=av_rescale_q(pkt->pts,ifmt_ctx->streams[pkt->stream_index]->time_base,AV_TIME_BASE_Q);
 task_handle_process_info->handled_rate=(float)task_handle_process_info->pass_duration/(float)task_handle_process_info->total_duration;
                av_log(ifmt_ctx,AV_LOG_INFO,"Finished frames: %"PRId64" Total frames:%"PRId64" Finished rate:%f\n",task_handle_process_info->pass_duration,task_handle_process_info->total_duration,task_handle_process_info->handled_rate);
                //转换time_base,从steam->enc_ctx
                if (pkt->data!=NULL) {
                    av_packet_rescale_ts(pkt,input_streams[stream_mapping[pkt->stream_index]]->st->time_base,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base);
                }
                //处理滤镜 
                ret=(*handle_video_complex_filter)(pkt,out_pkt,frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,&output_streams[stream_mapping[pkt->stream_index]]->enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping[pkt->stream_index],handle_interleaved_write_frame,input_streams,stream_mapping,filter_graph,mainsrc_ctx,logo_ctx,subtitle_ctx,resultsink_ctx,filter_graph_des, output_streams);
                if (ret == AVERROR(EAGAIN)) {

                    continue;
                }else if (ret != 0) {
                    av_log(ifmt_ctx,AV_LOG_ERROR, "handle video codec faile\n");

                    continue;
                }else{

                    continue;
                }
            }

           //处理音频解编码
           if(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO&&(handle_audio_codec!=NULL||handle_audio_complex_filter!=NULL)){

   log_packet(ifmt_ctx, pkt, "in");

               //转换time_base,从steam->enc_ctx
               if (pkt->data!=NULL) {
                   av_packet_rescale_ts(pkt,input_streams[stream_mapping[pkt->stream_index]]->st->time_base,output_streams[stream_mapping[pkt->stream_index]]->enc_ctx->time_base);
               }

               ret=audio_pts=(*handle_audio_complex_filter)(pkt,out_pkt,frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,&output_streams[stream_mapping[pkt->stream_index]]->enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping[pkt->stream_index],handle_interleaved_write_frame,input_streams,stream_mapping,audio_pts,output_streams,abuffersrc_ctx,abuffersink_ctx,afilter_graph,afilters_descr );


               if (ret == AVERROR(EAGAIN)) {
                   continue;
               }else if (ret < 0) {
                   av_log(ifmt_ctx,AV_LOG_ERROR, "handle audio codec faile\n");

                   continue;
               }
               continue;

          }
          //处理字幕解编码
          if(ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_SUBTITLE&&(handle_subtitle_codec!=NULL)){

   log_packet(ifmt_ctx, pkt, "in");

               //转换time_base,从steam->enc_ctx
               /*if (pkt->data!=NULL) {
                   av_packet_rescale_ts(pkt,input_streams[stream_mapping[pkt->stream_index]]->st->time_base,AV_TIME_BASE_Q);
               }
*/
               ret=(*handle_subtitle_codec)(pkt,sub_frame,input_streams[stream_mapping[pkt->stream_index]]->dec_ctx,ifmt_ctx,stream_mapping,input_streams,output_streams,filter_graph_des->ass );


               if (ret == AVERROR(EAGAIN)) {
                   continue;
               }else if (ret < 0) {
                   av_log(ifmt_ctx,AV_LOG_ERROR, "handle audio codec faile\n");

                   continue;
               }
               continue;
          }
       
    }else{
      out_pkt=pkt;
      //直接输出packet 
      ret=simple_interleaved_write_frame_func(pkt,out_pkt,frame,dec_ctx,&enc_ctx,ifmt_ctx,ofmt_ctx,stream_mapping,output_streams);
      if(ret<0){
          av_log(ofmt_ctx,AV_LOG_ERROR,"interleaved write error:%d",ret);
          goto end;
      }
    }
        av_packet_unref(pkt);
        av_packet_unref(out_pkt);
        av_frame_unref(frame);
    }

end:
    av_frame_free(&frame);
    av_frame_free(&new_logo_frame);
   
    if(sub_frame)
       free_subtitle_frame(sub_frame);
    

    avformat_close_input(&ifmt_ctx);
    avformat_free_context(ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    av_write_trailer(ofmt_ctx);

    avformat_free_context(ofmt_ctx);

    av_packet_free(&pkt);
    av_packet_free(&out_pkt);

    //释放输入流的解码AVCodec_Context

    for(int i=0;i<nb_input_streams;i++){

        //av_buffer_unref(&input_streams[i]->dec_ctx->hw_device_ctx);
        //avcodec_close(input_streams[i]->dec_ctx);

        avcodec_free_context(&input_streams[i]->dec_ctx);

    }
    //释放输出流的编码AVCodec_Context
    for(int i=0;i<nb_output_streams;i++){
        //av_buffer_unref(&output_streams[i]->enc_ctx->hw_frames_ctx);
        //avcodec_close(output_streams[i]->enc_ctx);

        avcodec_free_context(&output_streams[i]->enc_ctx);

    }
    
   
    av_buffer_unref(hw_device_ctx);

   
    if(ret!=AVERROR(ECONNREFUSED)){

        avfilter_free(*mainsrc_ctx);
        avfilter_free(*logo_ctx);
        avfilter_free(*resultsink_ctx);

       
    }

     
    //释放滤镜
    avfilter_graph_free(filter_graph);
    //if(afilter_graph)
    avfilter_free(*abuffersrc_ctx);
    avfilter_free(*abuffersink_ctx);

    avfilter_graph_free(afilter_graph);
    if(filter_graph_des->ass) 
        av_free(filter_graph_des->ass);
    
    if(filter_graph_des->overlay_ctx)
        av_free(filter_graph_des->overlay_ctx);

    if(filter_graph_des->subtitle_path)
        av_free(filter_graph_des->subtitle_path);
    if(filter_graph_des)
        av_free(filter_graph_des);

    //av_freep只能用于指向指针的指针,因为还需要对指针赋NULL，否则找不到地址

    av_freep(input_streams);
    av_freep(output_streams);
    

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return ret;
    }

    return 0;

}
int hw_device_setup_for_filter(FilterGraph *fg,AVBufferRef *hw_device_ctx)
{
           for (int i = 0; i < fg->graph->nb_filters; i++) {
            fg->graph->filters[i]->hw_device_ctx =
                av_buffer_ref(hw_device_ctx);
            if (!fg->graph->filters[i]->hw_device_ctx)
                return AVERROR(ENOMEM);
        }
  
    return 0;
}
int aligned_frame_height(int h){

  return ENCODE_ALIGN_BITS*(h / ENCODE_ALIGN_BITS);

};

int set_hwframe_ctx_cuda(AVCodecContext *ctx, AVFilterContext *filter)
{
//AVHWFramesContext *frames_ctx = NULL;

  /*  AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;
    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        fprintf(stderr, "Failed to create VAAPI frame context.\n");
        return -1;
    }*/
 //int err = 0;
    //frames_ctx = (AVHWFramesContext *)(hw_device_ctx->data);
    //frames_ctx->format    = AV_PIX_FMT_CUDA;
    //frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
    //frames_ctx->width     = ctx->width;
    //frames_ctx->height    = ctx->height;
    //frames_ctx->initial_pool_size = 20;
    //if ((err = av_hwframe_ctx_init(hw_device_ctx)) < 0) {
 //       fprintf(stderr, "Failed to initialize VAAPI frame context."
    //            "Error code: %s\n",av_err2str(err));
        //av_buffer_unref(&hw_frames_ref);
  //      return err;
   // }

/*int err = 0;
  
    ctx->hw_device_ctx=av_buffer_ref(*hw_device_ctx);
      printf("################################ %d \n",(*hw_device_ctx)->data);
  ctx->hw_frames_ctx = av_buffer_ref((*hw_device_ctx)->data);

    if (!ctx->hw_frames_ctx){

      //printf("9999999999999999999999999");
        err = AVERROR(ENOMEM);
    }
    //av_buffer_unref(&hw_frames_ref);
*/
    //return err;
   AVBufferRef *frames_ref=NULL;

   if(filter){

     frames_ref=av_buffersink_get_hw_frames_ctx(filter);

 printf("################################ %d \n",frames_ref);
     ctx->hw_frames_ctx=av_buffer_ref(frames_ref);


   } 

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

    TaskHandleProcessInfo *info=task_handle_process_info_alloc();
    info->video_codec_id=AV_CODEC_ID_HEVC;
    //info->height=1080;
    //info->width=1920;
    //info->bit_rate= 400000;
    //info->bit_rate= -1;
    //
    info->audio_channels=6;
    //info->control=av_mallocz(sizeof(*TaskHandleProcessControl));
    //info->control->subtitle_time_offset=2000;
    info->control->subtitle_charenc="GBK";
//printf("##################1\n");
    //info->control->seek_time=300;


   // info->control->seek_time=1;
    
    //ret=push_video_to_rtsp_subtitle_logo(in_filename,atoi(argv[4]),atoi(argv[5]), subtitle_filename, &logo_frame,rtsp_path,if_hw,true,480,480*6,480*3,info);
    ret=push2rtsp_sub_logo_cuda(in_filename,atoi(argv[4]),atoi(argv[5]), atoi(argv[6]),subtitle_filename, &logo_frame,rtsp_path,if_hw,true,480,480*6,480*3,info);


    //task_handle_process_info_free(info) ;  
    return 0;
}

