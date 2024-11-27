
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ass/ass.h>
#include "subtitle.h"
#include <libavutil/attributes.h>
#include <libavutil/cpu.h>
#include "drawutils.h"

//#include "x86/cpu.h"
//#include "libavfilter/vf_overlay.h"

/**
* 字符串originString以字符串prefix开头，返回0；否则返回1；异常返回0
*/
int startWith(const char *originString, char *prefix) {
    // 参数校验
    if (originString == NULL || prefix == NULL || strlen(prefix) > strlen(originString)) {
        printf("参数异常，请重新输入！\n");
        return -1;
    }
    
    int n = strlen(prefix);
    int i;
    for (i = 0; i < n; i++) {
        if (originString[i] != prefix[i]) {
            return 1;
        }
    }
    return 0;
}

/**
* 字符串originString以字符串end结尾，返回0；否则返回1；异常返回0
*/
int endWith(const char *originString, char *end) {
    // 参数校验
    if (originString == NULL || end == NULL || strlen(end) > strlen(originString)) {
        printf("参数异常，请重新输入！\n");
        return -1;
    }
    
    int n = strlen(end);
    int m = strlen(originString);
    int i;
    for (i = 0; i < n; i++) {
        if (originString[m-i-1] != end[n-i-1]) {
            return 1;
        }
    }
    return 0;
}

int ff_overlay_row_44_sse4(uint8_t *d, uint8_t *da, uint8_t *s, uint8_t *a,
                           int w, ptrdiff_t alinesize);

int ff_overlay_row_20_sse4(uint8_t *d, uint8_t *da, uint8_t *s, uint8_t *a,
                           int w, ptrdiff_t alinesize);

int ff_overlay_row_22_sse4(uint8_t *d, uint8_t *da, uint8_t *s, uint8_t *a,
                           int w, ptrdiff_t alinesize);

av_cold void ff_overlay_init_x862(OverlayContext *s, int format, int pix_format,
                                 int alpha_format, int main_has_alpha)
{
    int cpu_flags = av_get_cpu_flags();

    if ((cpu_flags&AV_CPU_FLAG_SSE4) &&
        (format == OVERLAY_FORMAT_YUV444 ||
         format == OVERLAY_FORMAT_GBRP) &&
        alpha_format == 0 && main_has_alpha == 0) {
        s->blend_row[0] = ff_overlay_row_44_sse4;
        s->blend_row[1] = ff_overlay_row_44_sse4;
        s->blend_row[2] = ff_overlay_row_44_sse4;
    }

    if ((cpu_flags&AV_CPU_FLAG_SSE4) &&
        (pix_format == AV_PIX_FMT_YUV420P) &&
        (format == OVERLAY_FORMAT_YUV420) &&
        alpha_format == 0 && main_has_alpha == 0) {
        s->blend_row[0] = ff_overlay_row_44_sse4;
        s->blend_row[1] = ff_overlay_row_20_sse4;
        s->blend_row[2] = ff_overlay_row_20_sse4;
    }

    if ((cpu_flags&AV_CPU_FLAG_SSE4) &&
        (format == OVERLAY_FORMAT_YUV422) &&
        alpha_format == 0 && main_has_alpha == 0) {
        s->blend_row[0] = ff_overlay_row_44_sse4;
        s->blend_row[1] = ff_overlay_row_22_sse4;
        s->blend_row[2] = ff_overlay_row_22_sse4;
    }
}

// divide by 255 and round to nearest
// apply a fast variant: (X+127)/255 = ((X+127)*257+257)>>16 = ((X+128)*257)>>16
#define FAST_DIV255(x) ((((x) + 128) * 257) >> 16)

// calculate the unpremultiplied alpha, applying the general equation:
// alpha = alpha_overlay / ( (alpha_main + alpha_overlay) - (alpha_main * alpha_overlay) )
// (((x) << 16) - ((x) << 9) + (x)) is a faster version of: 255 * 255 * x
// ((((x) + (y)) << 8) - ((x) + (y)) - (y) * (x)) is a faster version of: 255 * (x + y)
#define UNPREMULTIPLY_ALPHA(x, y) ((((x) << 16) - ((x) << 9) + (x)) / ((((x) + (y)) << 8) - ((x) + (y)) - (y) * (x)))

static const enum AVPixelFormat alpha_pix_fmts[] = {
    AV_PIX_FMT_YUVA420P, AV_PIX_FMT_YUVA422P, AV_PIX_FMT_YUVA444P,
    AV_PIX_FMT_ARGB, AV_PIX_FMT_ABGR, AV_PIX_FMT_RGBA,
    AV_PIX_FMT_BGRA, AV_PIX_FMT_GBRAP, AV_PIX_FMT_NONE
};

void msg_callback(int level, const char *fmt, va_list va, void *data)
{
    if (level > 6)
        return;
    printf("libass: ");
    vprintf(fmt, va);
    printf("\n");
}
extern void ff_overlay_init_x86(OverlayContext *s, int format, int pix_format, int alpha_format, int main_has_alpha);
static void init(int frame_w, int frame_h,ASS_Library **ass_library_point,ASS_Renderer **ass_renderer_point)
{
    ASS_Library *ass_library = ass_library_init();

    ass_library_point=&ass_library;
    if (!ass_library) {
        printf("ass_library_init failed!\n");
        exit(1);
    }

    ass_set_message_cb(ass_library, msg_callback, NULL);
    ass_set_extract_fonts(ass_library, 1);

    ASS_Library *ass_renderer = ass_renderer_init(ass_library);

    ass_renderer_point=&ass_renderer;
    if (!ass_renderer) {
        printf("ass_renderer_init failed!\n");
        exit(1);
    }

    ass_set_storage_size(ass_renderer, frame_w, frame_h);
    ass_set_frame_size(ass_renderer, frame_w, frame_h);
    ass_set_fonts(ass_renderer, NULL, "sans-serif",
                  ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
}



#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

static void blend_single(image_t * frame, ASS_Image *img)
{
    int x, y;
    unsigned char opacity = 255 - _a(img->color);
    unsigned char r = _r(img->color);
    unsigned char g = _g(img->color);
    unsigned char b = _b(img->color);

    unsigned char *src;
    unsigned char *dst;

    src = img->bitmap;
 printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$%d %d %d\n",0,frame->height,img->dst_y);
    //dst = frame->buffer + (img->dst_y-(origin_h-frame->height)) * frame->stride + img->dst_x * 4;
//dst = frame->buffer + img->dst_y * frame->stride + img->dst_x * 4;
dst=frame->buffer;
    for (y = 0; y < img->h; ++y) {
 printf("qqqqqqqqqqqqqqqqqqqqqqqq2\n");
        for (x = 0; x < img->w; ++x) {
            unsigned k = ((unsigned) src[x]) * opacity / 255;
            // possible endianness problems
            dst[x * 4+3] = (k * opacity + (255 - k) * dst[x * 4+3]) / 255;
            dst[x * 4] = (k * b + (255 - k) * dst[x * 4]) / 255;
            dst[x *  4+ 1] = (k * g + (255 - k) * dst[x * 4 + 1]) / 255;
            dst[x * 4 + 2] = (k * r + (255 - k) * dst[x * 4 + 2]) / 255;
        }
        src += img->stride;
        dst += frame->stride;
    }
}
static void blend_single2(image_t * frame, ASS_Image *img)
{
    int x, y;
    unsigned char opacity = 255 - _a(img->color);
    unsigned char r = _r(img->color);
    unsigned char g = _g(img->color);
    unsigned char b = _b(img->color);

    unsigned char *src;
    unsigned char *dst;

    src = img->bitmap;
 //printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$%d %d %d %d\n",img->w,img->stride,img->h,img->dst_x);
    //dst = frame->buffer + (img->dst_y-(origin_h-frame->height)) * frame->stride + img->dst_x * 4;
//dst = frame->buffer + img->dst_y * frame->stride + img->dst_x * 4;
dst=frame->buffer;
    for (y = 0; y < img->h; ++y) {
// printf("qqqqqqqqqqqqqqqqqqqqqqqq2\n");
        for (x = 0; x < img->w; ++x) {
            unsigned k = ((unsigned) src[x]) * opacity / 255;
            // possible endianness problems
            dst[x * 4+3] = (k * opacity ) / 255;
            dst[x * 4] = (k * b ) / 255;
            dst[x *  4+ 1] = (k * g ) / 255;
            dst[x * 4 + 2] = (k * r ) / 255;
        }
        src += img->stride;
        dst += frame->stride;
    }
}
void blend(image_t * frame, ASS_Image *img)
{
    int cnt = 0;
    ASS_Image *new_img=img;
    while (new_img) {
        blend_single2(frame, new_img);
        ++cnt;

        //old_img=img;
        new_img = new_img->next;
//if(old_img)free(old_img);

    }

    //if(img)free(img);

       // printf("%d images blended\n", cnt);
}



char *font_provider_labels[] = {
    [ASS_FONTPROVIDER_NONE]       = "None",
    [ASS_FONTPROVIDER_AUTODETECT] = "Autodetect",
    [ASS_FONTPROVIDER_CORETEXT]   = "CoreText",
    [ASS_FONTPROVIDER_FONTCONFIG] = "Fontconfig",
    [ASS_FONTPROVIDER_DIRECTWRITE]= "DirectWrite",
};

static void print_font_providers(ASS_Library *ass_library)
{
    int i;
    ASS_DefaultFontProvider *providers;
    size_t providers_size = 0;
    ass_get_available_font_providers(ass_library, &providers, &providers_size);
    printf("test.c: Available font providers (%zu): ", providers_size);
    for (i = 0; i < providers_size; i++) {
        const char *separator = i > 0 ? ", ": "";
        printf("%s'%s'", separator,  font_provider_labels[providers[i]]);
    }
    printf(".\n");
    free(providers);
}


static av_always_inline void blend_plane(OverlayContext *octx,
                                         AVFrame *dst, const AVFrame *src,
                                         int src_w, int src_h,
                                         int dst_w, int dst_h,
                                         int i, int hsub, int vsub,
                                         int x, int y,
                                         int main_has_alpha,
                                         int dst_plane,
                                         int dst_offset,
                                         int dst_step,
                                         int straight,
                                         int yuv,
                                         int jobnr,
                                         int nb_jobs)
{
    //OverlayContext *octx = ctx->priv;
    int src_wp = AV_CEIL_RSHIFT(src_w, hsub);
    int src_hp = AV_CEIL_RSHIFT(src_h, vsub);
    int dst_wp = AV_CEIL_RSHIFT(dst_w, hsub);
    int dst_hp = AV_CEIL_RSHIFT(dst_h, vsub);
    int yp = y>>vsub;
    int xp = x>>hsub;
    uint8_t *s, *sp, *d, *dp, *dap, *a, *da, *ap;
    int jmax, j, k, kmax;
    int slice_start, slice_end;

    j = FFMAX(-yp, 0);
    jmax = FFMIN3(-yp + dst_hp, FFMIN(src_hp, dst_hp), yp + src_hp);

    slice_start = j + (jmax * jobnr) / nb_jobs;
    slice_end = j + (jmax * (jobnr+1)) / nb_jobs;

    sp = src->data[i] + (slice_start) * src->linesize[i];
    dp = dst->data[dst_plane]
                      + (yp + slice_start) * dst->linesize[dst_plane]
                      + dst_offset;
    ap = src->data[3] + (slice_start << vsub) * src->linesize[3];
    dap = dst->data[3] + ((yp + slice_start) << vsub) * dst->linesize[3];
//printf("$$$$$$$$$$$$$$$$ %d %d  \n",slice_start,slice_end);
    for (j = slice_start; j < slice_end; j++) {
        k = FFMAX(-xp, 0);
        d = dp + (xp+k) * dst_step;
        s = sp + k;
        a = ap + (k<<hsub);
        da = dap + ((xp+k) << hsub);
        kmax = FFMIN(-xp + dst_wp, src_wp);
//printf("$$$$$$$$$$$$$$$$1 %d\n",octx->blend_row);
        if (((vsub && j+1 < src_hp) || !vsub) && octx->blend_row[i]) {
//printf("$$$$$$$$$$$$$$$$ %d %d %d %d %d %d %d \n",d, da, s, a, kmax , k, src->linesize[3]);
            int c = octx->blend_row[i](d, da, s, a, kmax - k, src->linesize[3]);
     //printf("###################\n");
            s += c;
            d += dst_step * c;
            da += (1 << hsub) * c;
            a += (1 << hsub) * c;
            k += c;
        }

     
   
        for (; k < kmax; k++) {
            int alpha_v, alpha_h, alpha;

            // average alpha for color components, improve quality
            if (hsub && vsub && j+1 < src_hp && k+1 < src_wp) {
                alpha = (a[0] + a[src->linesize[3]] +
                         a[1] + a[src->linesize[3]+1]) >> 2;
            } else if (hsub || vsub) {
                alpha_h = hsub && k+1 < src_wp ?
                    (a[0] + a[1]) >> 1 : a[0];
                alpha_v = vsub && j+1 < src_hp ?
                    (a[0] + a[src->linesize[3]]) >> 1 : a[0];
                alpha = (alpha_v + alpha_h) >> 1;
            } else
                alpha = a[0];
            // if the main channel has an alpha channel, alpha has to be calculated
            // to create an un-premultiplied (straight) alpha value
        
            if (main_has_alpha && alpha != 0 && alpha != 255) {
                // average alpha for color components, improve quality
                uint8_t alpha_d;
                if (hsub && vsub && j+1 < src_hp && k+1 < src_wp) {
                    alpha_d = (da[0] + da[dst->linesize[3]] +
                               da[1] + da[dst->linesize[3]+1]) >> 2;
                } else if (hsub || vsub) {
                    alpha_h = hsub && k+1 < src_wp ?
                        (da[0] + da[1]) >> 1 : da[0];
                    alpha_v = vsub && j+1 < src_hp ?
                        (da[0] + da[dst->linesize[3]]) >> 1 : da[0];
                    alpha_d = (alpha_v + alpha_h) >> 1;
                } else
                    alpha_d = da[0];
                alpha = UNPREMULTIPLY_ALPHA(alpha, alpha_d);
            }
            if (straight) {
                *d = FAST_DIV255(*d * (255 - alpha) + *s * alpha);
            } else {
                if (i && yuv)
                    *d = av_clip(FAST_DIV255((*d - 128) * (255 - alpha)) + *s - 128, -128, 128) + 128;
                else
                    *d = FFMIN(FAST_DIV255(*d * (255 - alpha)) + *s, 255);
            }
            s++;
            d += dst_step;
            da += 1 << hsub;
            a += 1 << hsub;
        }
        dp += dst->linesize[dst_plane];
        sp += src->linesize[i];
        ap += (1 << vsub) * src->linesize[3];
        dap += (1 << vsub) * dst->linesize[3];
    }
}
static inline void alpha_composite(const AVFrame *src, const AVFrame *dst,
                                   int src_w, int src_h,
                                   int dst_w, int dst_h,
                                   int x, int y,
                                   int jobnr, int nb_jobs)
{
    uint8_t alpha;          ///< the amount of overlay to blend on to main
    uint8_t *s, *sa, *d, *da;
    int i, imax, j, jmax;
    int slice_start, slice_end;

    imax = FFMIN(-y + dst_h, src_h);
    slice_start = (imax * jobnr) / nb_jobs;
    slice_end = ((imax * (jobnr+1)) / nb_jobs);

    i = FFMAX(-y, 0);
    sa = src->data[3] + (i + slice_start) * src->linesize[3];
    da = dst->data[3] + (y + i + slice_start) * dst->linesize[3];

    for (i = i + slice_start; i < slice_end; i++) {
        j = FFMAX(-x, 0);
        s = sa + j;
        d = da + x+j;

        for (jmax = FFMIN(-x + dst_w, src_w); j < jmax; j++) {
            alpha = *s;
            if (alpha != 0 && alpha != 255) {
                uint8_t alpha_d = *d;
                alpha = UNPREMULTIPLY_ALPHA(alpha, alpha_d);
            }
            switch (alpha) {
            case 0:
                break;
            case 255:
                *d = *s;
                break;
            default:
                // apply alpha compositing: main_alpha += (1-main_alpha) * overlay_alpha
                *d += FAST_DIV255((255 - *d) * *s);
            }
            d += 1;
            s += 1;
        }
        da += dst->linesize[3];
        sa += src->linesize[3];
    }
}


static av_always_inline void blend_slice_yuv(OverlayContext *s ,
                                             AVFrame *dst, const AVFrame *src,
                                             int hsub, int vsub,
                                             int main_has_alpha,
                                             int x, int y,
                                             int is_straight,
                                             int jobnr, int nb_jobs)
{
    //OverlayContext *s = ctx->priv;
    const int src_w = src->width;
    const int src_h = src->height;
    const int dst_w = dst->width;
    const int dst_h = dst->height;

    blend_plane(s, dst, src, src_w, src_h, dst_w, dst_h, 0, 0,       0, x, y, main_has_alpha,
                s->main_desc->comp[0].plane, s->main_desc->comp[0].offset, s->main_desc->comp[0].step, is_straight, 1,
                jobnr, nb_jobs);

    blend_plane(s, dst, src, src_w, src_h, dst_w, dst_h, 1, hsub, vsub, x, y, main_has_alpha,
                s->main_desc->comp[1].plane, s->main_desc->comp[1].offset, s->main_desc->comp[1].step, is_straight, 1,
                jobnr, nb_jobs);
    blend_plane(s, dst, src, src_w, src_h, dst_w, dst_h, 2, hsub, vsub, x, y, main_has_alpha,
                s->main_desc->comp[2].plane, s->main_desc->comp[2].offset, s->main_desc->comp[2].step, is_straight, 1,
                jobnr, nb_jobs);

    if (main_has_alpha)
        alpha_composite(src, dst, src_w, src_h, dst_w, dst_h, x, y, jobnr, nb_jobs);
}
static  void blend_slice_packed_rgb( AVFrame *dst, const AVFrame *src,
                                   int main_has_alpha, int x, int y,
                                   int is_straight, int jobnr, int nb_jobs)
{
    //OverlayContext *s = ctx->priv;
    int i, imax, j, jmax;
    const int src_w = src->width;
    const int src_h = src->height;
    const int dst_w = dst->width;
    const int dst_h = dst->height;
    uint8_t alpha;          ///< the amount of overlay to blend on to main
    const int dr = 0x02;//s->main_rgba_map[R];
    const int dg = 0x01;//s->main_rgba_map[G];
    const int db = 0x00;//s->main_rgba_map[B];
    const int da = 0x03;//s->main_rgba_map[A];
    const int dstep = 0x04;//s->main_pix_step[0];
    const int sr = 0x02;//s->overlay_rgba_map[R];
    const int sg = 0x01;//s->overlay_rgba_map[G];
    const int sb = 0x00;//s->overlay_rgba_map[B];
    const int sa = 0x03;//s->overlay_rgba_map[A];
    const int sstep = 0x04;//s->overlay_pix_step[0];
    int slice_start, slice_end;
    uint8_t *S, *sp, *d, *dp;

    i = FFMAX(-y, 0);
    imax = FFMIN3(-y + dst_h, FFMIN(src_h, dst_h), y + src_h);

    slice_start = i + (imax * jobnr) / nb_jobs;
    slice_end = i + (imax * (jobnr+1)) / nb_jobs;

    sp = src->data[0] + (slice_start)     * src->linesize[0];
    dp = dst->data[0] + (y + slice_start) * dst->linesize[0];

    for (i = slice_start; i < slice_end; i++) {
        j = FFMAX(-x, 0);
        S = sp + j     * sstep;
        d = dp + (x+j) * dstep;

        for (jmax = FFMIN(-x + dst_w, src_w); j < jmax; j++) {
            alpha = S[sa];

            // if the main channel has an alpha channel, alpha has to be calculated
            // to create an un-premultiplied (straight) alpha value
            if (main_has_alpha && alpha != 0 && alpha != 255) {
                uint8_t alpha_d = d[da];
                alpha = UNPREMULTIPLY_ALPHA(alpha, alpha_d);
            }

            switch (alpha) {
            case 0:
                break;
            case 255:
                d[dr] = S[sr];
                d[dg] = S[sg];
                d[db] = S[sb];
                break;
            default:
                // main_value = main_value * (1 - alpha) + overlay_value * alpha
                // since alpha is in the range 0-255, the result must divided by 255
                d[dr] = is_straight ? FAST_DIV255(d[dr] * (255 - alpha) + S[sr] * alpha) :
                        FFMIN(FAST_DIV255(d[dr] * (255 - alpha)) + S[sr], 255);
                d[dg] = is_straight ? FAST_DIV255(d[dg] * (255 - alpha) + S[sg] * alpha) :
                        FFMIN(FAST_DIV255(d[dg] * (255 - alpha)) + S[sg], 255);
                d[db] = is_straight ? FAST_DIV255(d[db] * (255 - alpha) + S[sb] * alpha) :
                        FFMIN(FAST_DIV255(d[db] * (255 - alpha)) + S[sb], 255);
            }
            if (main_has_alpha) {
                switch (alpha) {
                case 0:
                    break;
                case 255:
                    d[da] = S[sa];
                    break;
                default:
                    // apply alpha compositing: main_alpha += (1-main_alpha) * overlay_alpha
                    d[da] += FAST_DIV255((255 - d[da]) * S[sa]);
                }
            }
            d += dstep;
            S += sstep;
        }
        dp += dst->linesize[0];
        sp += src->linesize[0];
    }
}

static int blend_slice_yuv420(OverlayContext *s, AVFrame *dst,AVFrame *src, int jobnr, int nb_jobs)
{
    //OverlayContext *s = ctx->priv;
  //printf("$$$$$$$$$$$$$$$234i %d %d %d\n",s,dst,src);
  blend_slice_yuv(s, dst, src, 1, 1, 0, s->x, s->y, 1, jobnr, nb_jobs);
    return 0;
}
static int blend_slice_rgba( AVFrame *dst,AVFrame *src, int x,int y,int jobnr, int nb_jobs)
{
    //OverlayContext *s = ctx->priv;
  //printf("$$$$$$$$$$$$$$$234i %d %d %d\n",s,dst,src);
  blend_slice_packed_rgb(dst, src,  0, x, y, 1, jobnr, nb_jobs);
    return 0;
}

int overlay_image(OverlayContext *s,AVFrame *dst,AVFrame *src,int x,int y){


  printf("src w:%d h:%d\n",src->width,src->height);
  s->x=x;
  s->y=y;
  //线程编号从0开始
  int ret=blend_slice_yuv420(s,dst,src,0, 1);

  if(ret<0){

  }

 
  

}

OverlayContext  *init_overlay_context(int frame_fmt){


  OverlayContext *overlay_ctx=av_malloc(sizeof(*overlay_ctx));




const AVPixFmtDescriptor *pix_desc_main = av_pix_fmt_desc_get(frame_fmt);

    av_image_fill_max_pixsteps(overlay_ctx->main_pix_step,    NULL, pix_desc_main);

    overlay_ctx->hsub = pix_desc_main->log2_chroma_w;
    overlay_ctx->vsub = pix_desc_main->log2_chroma_h;

    overlay_ctx->main_desc = pix_desc_main;

    overlay_ctx->main_is_packed_rgb =
        ff_fill_rgba_map(overlay_ctx->main_rgba_map, frame_fmt) >= 0;
    overlay_ctx->main_has_alpha = ff_fmt_is_in(frame_fmt, alpha_pix_fmts);
    //overlay_ctx->x=100;
    //overlay_ctx->y=100;
  printf("cpu_flags:%d\n",av_get_cpu_flags());

ff_overlay_init_x862(overlay_ctx,OVERLAY_FORMAT_YUV420, AV_PIX_FMT_YUV420P,0,overlay_ctx->main_has_alpha);//1 ,  overlay_ctx->main_has_alpha);


printf("blend_row %d %d %d \n",overlay_ctx->blend_row[0],overlay_ctx->blend_row[1],overlay_ctx->blend_row[2]);
    
    const AVPixFmtDescriptor *pix_desc = av_pix_fmt_desc_get(AV_PIX_FMT_YUVA420P);

    av_image_fill_max_pixsteps(overlay_ctx->overlay_pix_step, NULL, pix_desc);

    /* Finish the configuration by evaluating the expressions
       now when both inputs are configured. */
    /*s->var_values[VAR_MAIN_W   ] = s->var_values[VAR_MW] = >w;
    s->var_values[VAR_MAIN_H   ] = s->var_values[VAR_MH] = ctx->inputs[MAIN   ]->h;
    s->var_values[VAR_OVERLAY_W] = s->var_values[VAR_OW] = ctx->inputs[OVERLAY]->w;
    s->var_values[VAR_OVERLAY_H] = s->var_values[VAR_OH] = ctx->inputs[OVERLAY]->h;
    s->var_values[VAR_HSUB]  = 1<<pix_desc->log2_chroma_w;
    s->var_values[VAR_VSUB]  = 1<<pix_desc->log2_chroma_h;
    s->var_values[VAR_X]     = NAN;
    s->var_values[VAR_Y]     = NAN;
    s->var_values[VAR_N]     = 0;
    s->var_values[VAR_T]     = NAN;
    s->var_values[VAR_POS]   = NAN;
*/
   
    overlay_ctx->overlay_is_packed_rgb =
        ff_fill_rgba_map(overlay_ctx->overlay_rgba_map, AV_PIX_FMT_RGBA) >= 0;
    overlay_ctx->overlay_has_alpha = ff_fmt_is_in(AV_PIX_FMT_YUVA420P, alpha_pix_fmts);
    return overlay_ctx;

}


void free_subtitle_frame(SubtitleFrame *subtitle_frame){
    if(subtitle_frame){
        if (subtitle_frame->sub_frame){
            av_frame_unref(subtitle_frame->sub_frame);


            av_frame_free(&(subtitle_frame->sub_frame));
        }

        av_free(subtitle_frame) ;

    }
}

SubtitleFrame *alloc_subtitle_frame(){


    SubtitleFrame *subtitle_frame=av_malloc(sizeof(*subtitle_frame));


    subtitle_frame->sub_frame=NULL;

    //if(subtitle_frame){

      return subtitle_frame;

    //}
    

}

image_t * gen_image(int width, int height)
{
    image_t *img = (image_t *)malloc(sizeof(image_t));
    img->width = width;
    img->height = height;
    img->stride = width * 4;
    img->buffer = calloc(1,img->height*img->stride);

    printf("memset buffer \n");
    //memset(img->buffer, 0x00, img->stride * img->height);

    //for(int i=0;i<img->height*img->stride;i++){

    //memset(img->buffer+i*img->stride, 0x01, 1);

    //}
    //for (int i = 0; i < height * width * 3; ++i)
    // img->buffer[i] = (i/3/50) % 100;
    return img;
}


int gen_empty_layout_frame(AVFrame **frame,int width, int height)
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
                //
                //
               *frame=tmp_frame; 
                //av_frame_unref(frame);
                //av_frame_move_ref(frame,tmp_frame);

                //av_frame_free(&tmp_frame);
                //
  //printf("memset buffer2 %d \n",tmp_frame);

                return 0;


}


