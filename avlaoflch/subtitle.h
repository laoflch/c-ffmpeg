#ifndef SUBTITLE_LAOFLCH_H
#define SUBTITLE_LAOFLCH_H
#include <ass/ass.h>
#include <libavformat/avformat.h>
#include <libavutil/eval.h>
#include <libavutil/pixdesc.h>

#include <libavfilter/avfilter.h>

//#include <libavfilter/vf_overlay.h>
//

#define LAOFMC_SUB_FILE_ASS_SUFFIX ".ass"

#define LAOFMC_SUB_FILE_ASS_SUFFIX ".srt"

enum var_name {
    VAR_MAIN_W,    VAR_MW,
    VAR_MAIN_H,    VAR_MH,
    VAR_OVERLAY_W, VAR_OW,
    VAR_OVERLAY_H, VAR_OH,
    VAR_HSUB,
    VAR_VSUB,
    VAR_X,
    VAR_Y,
    VAR_N,
    VAR_POS,
    VAR_T,
    VAR_VARS_NB
};

enum OverlayFormat {
    OVERLAY_FORMAT_YUV420,
    OVERLAY_FORMAT_YUV422,
    OVERLAY_FORMAT_YUV444,
    OVERLAY_FORMAT_RGB,
    OVERLAY_FORMAT_GBRP,
    OVERLAY_FORMAT_AUTO,
    OVERLAY_FORMAT_NB
};
enum SubtitleType {
    SUBTITLE_TYPE_NULL,
    SUBTITLE_TYPE_ASS,
    SUBTITLE_TYPE_SRT,
    SUBTITLE_TYPE_PGS

};


typedef struct OverlayContext {
    const AVClass *class;
    int x, y;                   ///< position of overlaid picture

    uint8_t main_is_packed_rgb;
    uint8_t main_rgba_map[4];
    uint8_t main_has_alpha;
    uint8_t overlay_is_packed_rgb;
    uint8_t overlay_rgba_map[4];
    uint8_t overlay_has_alpha;
    int format;                 ///< OverlayFormat
    int alpha_format;
    int eval_mode;              ///< EvalMode

   // FFFrameSync fs;

    int main_pix_step[4];       ///< steps per pixel for each plane of the main output
    int overlay_pix_step[4];    ///< steps per pixel for each plane of the overlay
    int hsub, vsub;             ///< chroma subsampling values
    const AVPixFmtDescriptor *main_desc; ///< format descriptor for main input

    double var_values[VAR_VARS_NB];
    char *x_expr, *y_expr;

    AVExpr *x_pexpr, *y_pexpr;

    int (*blend_row[4])(uint8_t *d, uint8_t *da, uint8_t *s, uint8_t *a, int w,
                        ptrdiff_t alinesize);
    //int (*blend_slice)(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs);
} OverlayContext;




int overlay_image(OverlayContext *s,AVFrame *dst,AVFrame *src,int x,int y);

typedef struct SubtitleFrame{
    AVFrame *sub_frame;
    int64_t pts;
    int64_t duration;

    int is_display;
} SubtitleFrame;

typedef struct image_s {
    int width, height, stride;
    unsigned char *buffer;      // ARGB24
} image_t;

OverlayContext  *init_overlay_context(int frame_fmt);
SubtitleFrame *alloc_subtitle_frame();
image_t *gen_image(int width, int height);
void free_subtitle_frame(SubtitleFrame *subtitle_frame);
int gen_empty_layout_frame(AVFrame **frame,int width, int height);
void blend(image_t * frame, ASS_Image *img);


int startWith(const char *originString, char *prefix);
int endWith(const char *originString, char *end) ;
#endif /*SUBTITLE_LAOFLCH_H */
