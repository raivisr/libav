#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"

#include "../internal.h"

static void yuv2rgb(void *ctx,
                    uint8_t *src[AVSCALE_MAX_COMPONENTS],
                    int sstrides[AVSCALE_MAX_COMPONENTS],
                    uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                    int dstrides[AVSCALE_MAX_COMPONENTS],
                    int w, int h)
{
    int i, j;
    int Y, U, V;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            //TODO coefficients & range
            Y = src[0][i];
            U = src[1][i >> 1] - 128;
            V = src[2][i >> 1] - 128;

            // offset for rgb/bgr are already applied
            dst[0][3 * i] = av_clip_uint8(Y + (             74698 * V + 32768 >> 16));
            dst[1][3 * i] = av_clip_uint8(Y + (-25861 * U - 38050 * V + 32768 >> 16));
            dst[2][3 * i] = av_clip_uint8(Y + (133180 * U             + 32768 >> 16));
#if 0
            av_log(ctx, AV_LOG_WARNING,
                   "0x%02X 0x%02X 0x%02X -> 0x%02X 0x%02X 0x%02X\n",
                   src[0][i],     src[1][i >> 1],    src[2][i >> 1],
                   dst[0][3 * i], dst[0][3 * i + 1], dst[0][3 * i + 2]);
#endif
        }
        src[0] += sstrides[0];
        if (j & 1) {
            src[1] += sstrides[1];
            src[2] += sstrides[2];
        }
        dst[0] += dstrides[0];
        dst[1] += dstrides[1];
        dst[2] += dstrides[2];
    }
}

static int yuv2rgb_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    if (ctx->cur_fmt->component[0].depth <= 8)
        stage->do_common = yuv2rgb;
    else
        return AVERROR(ENOSYS);


    return 0;
}

const AVScaleKernel avs_yuv2rgb_kernel = {
    .name = "yuv2rgb",
    .kernel_init = yuv2rgb_kernel_init,
};
