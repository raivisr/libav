/*
 * Copyright (c) 2015 Kostya Shishkov
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdint.h>
#include <string.h>

#include "libavutil/avstring.h"
#include "libavutil/common.h"
#include "libavutil/mem.h"
#include "internal.h"

AVScaleContext *avscale_alloc_context(void)
{
    return av_mallocz(sizeof(AVScaleContext));
}

static int prepare_next_stage(AVScaleContext *ctx, AVScaleFilterStage **stage,
                              const char *name)
{
    int i, ret;
    AVScaleFilterStage *s;

    s = av_mallocz(sizeof(*s));
    if (!s)
        return AVERROR(ENOMEM);

    if (!ctx->head)
        ctx->head = s;

    for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
        s->w[i] = AV_CEIL_RSHIFT(ctx->cur_w, ctx->cur_fmt->component[i].h_sub);
        s->h[i] = AV_CEIL_RSHIFT(ctx->cur_h, ctx->cur_fmt->component[i].v_sub);
    }

    /* normally you're building chain like this:
     *   input =/-> [kernel] => [kernel] => [kernel] =/-> output
     * where => means planar data and =/-> means planar or packed data
     * so when you insert second or any following kernel you should allocate
     * temporary planar buffers for the previous kernel to output results to
     * all intermediate processing is done in planar form after all
     * (because I say so). */
    if (*stage) {
        for (i = 0; i < ctx->cur_fmt->nb_components + (ctx->dst_fmt->nb_components == 4); i++) {
            int w = AV_CEIL_RSHIFT(ctx->cur_w,
                                   ctx->cur_fmt->component[i].h_sub);
            int h = AV_CEIL_RSHIFT(ctx->cur_h,
                                   ctx->cur_fmt->component[i].v_sub);
            int dstride = (w + 31) & ~31;

            (*stage)->dst[i] = av_mallocz(h * dstride);
            if (!(*stage)->dst[i])
                return AVERROR(ENOMEM);
            (*stage)->dst_stride[i] = dstride;
            av_log(NULL, AV_LOG_INFO | AV_LOG_C(134),
                   "stage %s: allocated %d bytes for dst[%d]\n",
                   name, h * dstride, i);
        }
    }

    av_log(ctx, AV_LOG_WARNING, "kernel %s\n", name);
    if ((ret = avscale_apply_kernel(ctx, name, s)) < 0)
        goto err;

    if (*stage)
        (*stage)->next = s;
    *stage = s;
    return 0;
err:
    if (s->deinit)
        s->deinit(s);
    av_free(s);
    return ret;
}

int avscale_supported_input(AVPixelFormaton *fmt)
{

    if (fmt->model != AVCOL_MODEL_RGB &&
        fmt->model != AVCOL_MODEL_YUV ||
        fmt->pixel_size > 16) {
        return 0;
    }

    return 1;
}

int avscale_supported_output(AVPixelFormaton *fmt)
{

    if (fmt->model != AVCOL_MODEL_RGB &&
        fmt->model != AVCOL_MODEL_YUV ||
        fmt->pixel_size > 16) {
        return 0;
    }

    return 1;
}

#if 0
    /* Same color model (RGB) */
    if (ctx->src_fmt->model == AVCOL_MODEL_RGB &&
        ctx->dst_fmt->model == AVCOL_MODEL_RGB) {
        /* RGB packed to planar */
        if ( ctx->src_fmt->component[0].packed &&
            !ctx->dst_fmt->component[0].packed) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
                goto end;
        /* Diffrent RGB format OR different sizes OR different order */
        } else if ((ctx->src_fmt->pixel_size != ctx->dst_fmt->pixel_size) ||
                   (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h) ||
                   (ctx->src_fmt->component[0].offset != ctx->dst_fmt->component[0].offset)) {
            if (ctx->src_fmt->component[0].packed)
                if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
                    goto end;
            if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h)
                if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    goto end;
            if (ctx->dst_fmt->component[0].packed)
                if ((ret = prepare_next_stage(ctx, &stage, "rgbpack")) < 0)
                    goto end;
        /* Same format */
        } else {
            if ((ret = prepare_next_stage(ctx, &stage, "murder")) < 0)
                goto end;
        }
    /* Same color model (YUV) */
    } else if (ctx->src_fmt->model == AVCOL_MODEL_YUV &&
               ctx->dst_fmt->model == AVCOL_MODEL_YUV) {
        /* Diffrent RGB format OR different sizes */
        if ((ctx->src_fmt->pixel_size != ctx->dst_fmt->pixel_size) ||
            (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h)) {
            if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h)
                if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    goto end;
        /* Same format */
        } else {
            if ((ret = prepare_next_stage(ctx, &stage, "murder")) < 0)
                goto end;
        }
    /* Input RGB, Output YUV */
    } else if (ctx->src_fmt->model == AVCOL_MODEL_RGB &&
               ctx->dst_fmt->model == AVCOL_MODEL_YUV) {
        if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
            goto end;
        if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h) {
            if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                goto end;
        }
        if ((ret = prepare_next_stage(ctx, &stage, "rgb2yuv")) < 0)
            goto end;
    /* Input YUV, Output RGB */
    } else if (ctx->src_fmt->model == AVCOL_MODEL_YUV &&
               ctx->dst_fmt->model == AVCOL_MODEL_RGB) {
        if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h) {
            if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                goto end;
        }
        if ((ret = prepare_next_stage(ctx, &stage, "yuv2rgb")) < 0)
            goto end;
        if (!ctx->dst_fmt->component[0].packed) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
                goto end;
        }
    } else {
        ret = AVERROR(ENOSYS);
        goto end;
    }

#endif


/*
 * Return 1 the src_fmt and dst_fmt are exactly the same.
 * The conversion step is just av_frame_ref
 */

static int is_matching_all(AVScaleContext *ctx)
{
    const AVPixelFormaton *src = ctx->src_fmt;
    const AVPixelFormaton *dst = ctx->dst_fmt;

    size_t off = offsetof(AVPixelFormaton, model);
    size_t len = sizeof(*src) - off;

    return !memcmp(src + off, dst + off, len);
}

static int is_planar(const AVPixelFormaton *fmt)
{
    int i;

    for (i = 0; i < fmt->nb_components; i++) {
        if (fmt->component[i].packed)
            return 0;
    }

    return 1;
}

/*
 * Is the format one of the supported intermediates?
 * Do we have a scale kernel we can use directly with this color model?
 *
 * TODO: update later with additional checks.
 */
static int is_compatible(const AVPixelFormaton *fmt)
{
    return fmt->model == AVCOL_MODEL_RGB ||
           fmt->model == AVCOL_MODEL_YUV;
}

/*
 * Moving from the input to the output does require
 * to convert the format?
 *
 * - One of the format is not one of the supported intermediate
 * - The color model is not matching
 *
 * TODO: Check the color matrix and gamma.
 */
static int need_conversion(AVScaleContext *ctx)
{
    const AVPixelFormaton *src = ctx->src_fmt;
    const AVPixelFormaton *dst = ctx->dst_fmt;

    return !is_compatible(src) ||
           !is_compatible(dst) ||
            src->model != dst->model;
}

static int is_matching_dimension(AVScaleContext *ctx)
{
    return ctx->cur_w == ctx->dst_w &&
           ctx->cur_h == ctx->dst_h;
}

/*
 * After scaling, is the amount of pixels larger?
 */
static int is_upscale(AVScaleContext *ctx)
{
    return ctx->cur_w * ctx->cur_h < ctx->dst_w * ctx->dst_h;
}


// TODO use a lookup table
static int prepare_conversion_stage(AVScaleContext *ctx,
                                    AVScaleFilterStage **stage)
{
    char buf[128] = "";

    const char *src = av_color_model_name(ctx->src_fmt->model);
    const char *dst = av_color_model_name(ctx->dst_fmt->model);

    if (!src || !dst)
        return AVERROR(EINVAL);

    av_strlcatf(buf, sizeof(buf), "%s2%s", src, dst);

    return prepare_next_stage(ctx, stage, buf);
}

int avscale_config(AVScaleContext *ctx, AVFrame *dst, const AVFrame *src)
{
    AVScaleFilterStage *stage = 0;
    int ret = 0, need_scaling = 0, need_upscaling = 0;
    AVPixelFormatonRef *src_fmt_ref = av_pixformaton_ref(src->formaton);
    AVPixelFormatonRef *dst_fmt_ref = av_pixformaton_ref(dst->formaton);

    if (!src_fmt_ref || !dst_fmt_ref) {
        ret = AVERROR(ENOSYS);
        goto fail;
    }

    ctx->src_fmt = src_fmt_ref->pf;
    ctx->dst_fmt = dst_fmt_ref->pf;
    ctx->cur_w   = src->width;
    ctx->cur_h   = src->height;
    ctx->dst_w   = dst->width;
    ctx->dst_h   = dst->height;
    ctx->cur_fmt = ctx->src_fmt;

    if (is_matching_all(ctx)) {
        if ((ret = prepare_next_stage(ctx, &stage, "murder")) < 0)
            goto fail;
        goto out;
    }

    if (!is_planar(ctx->src_fmt)) {
        if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
            goto fail;
    }

    need_scaling = !is_matching_dimension(ctx);
    if (need_scaling)
        need_upscaling = is_upscale(ctx);

    if (need_conversion(ctx)) {
        if (need_scaling) {
            if (need_upscaling) {
                if ((ret = prepare_conversion_stage(ctx, &stage)) < 0)
                    goto fail;
                if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    goto fail;
            } else {
                if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    goto fail;
                if ((ret = prepare_conversion_stage(ctx, &stage)) < 0)
                    goto fail;
            }
        } else {
            if ((ret = prepare_conversion_stage(ctx, &stage)) < 0)
                goto fail;
        }
    } else {
        if (need_scaling) {
            if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    goto fail;
        }
    }

    if (!is_planar(ctx->dst_fmt)) {
        if ((ret = prepare_next_stage(ctx, &stage, "rgbpack")) < 0)
            goto fail;
    }

out:
    ctx->tail = stage;

fail:
    av_pixformaton_unref(&src_fmt_ref);
    av_pixformaton_unref(&dst_fmt_ref);

    return ret;
}

uint8_t *avscale_get_component_ptr(const AVFrame *src, int component_id)
{ // currently a simple hack - it has to be extended for e.g. NV12
    if (component_id >= src->formaton->pf->nb_components)
        return 0;
    if (!src->formaton->pf->component[component_id].packed)
        return src->data[src->formaton->pf->component[component_id].plane];
    else
        return src->data[0] + src->formaton->pf->component[component_id].offset;
}

int avscale_get_component_stride(const AVFrame *src, int component_id)
{
    if (src->linesize[component_id])
        return src->linesize[component_id];
    else
        return src->linesize[0];
}

int avscale_convert_frame(AVScaleContext *ctx,
                          AVFrame *dstf, const AVFrame *srcf)
{
    int ret;
    const AVScaleFilterStage *stage;

    int i;

    uint8_t *src[AVSCALE_MAX_COMPONENTS];
    int  sstride[AVSCALE_MAX_COMPONENTS];
    uint8_t *dst[AVSCALE_MAX_COMPONENTS];
    int  dstride[AVSCALE_MAX_COMPONENTS];
    uint8_t *src2[AVSCALE_MAX_COMPONENTS];
    uint8_t *dst2[AVSCALE_MAX_COMPONENTS];

    if (!ctx->head) {
        if ((ret = avscale_config(ctx, dstf, srcf)) < 0)
            return ret;
        av_log(ctx, AV_LOG_INFO, "build chain ret = %d\n", ret);
    }

    stage = ctx->head;

    for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
        src[i]     = avscale_get_component_ptr(srcf, i);
        sstride[i] = avscale_get_component_stride(srcf, i);
    }

    while (stage) {
        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
            if (stage->src[i]) {
                src[i]     = stage->src[i];
                sstride[i] = stage->src_stride[i];
            }
            if (stage->dst[i]) {
                dst[i]     = stage->dst[i];
                dstride[i] = stage->dst_stride[i];
            } else {
                dst[i]     = avscale_get_component_ptr(dstf, i);
                dstride[i] = avscale_get_component_stride(dstf, i);
            }
        }
        /* copy pointers */
        memcpy(src2, src, sizeof(src2));
        memcpy(dst2, dst, sizeof(dst2));

        if (stage->do_common)
            stage->do_common(stage->do_common_ctx,
                             src2, sstride, dst2, dstride,
                             stage->w[0], stage->h[0]);

        /* copy pointers again since do_common might have changed them */
        memcpy(src2, src, sizeof(src2));
        memcpy(dst2, dst, sizeof(dst2));

        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++)
            if (stage->do_component[i])
                stage->do_component[i](stage->do_component_ctx[i],
                                       src2[i], sstride[i],
                                       dst2[i], dstride[i],
                                       stage->w[i], stage->h[i]);

        // this stage output buffers are likely to be next stage input
        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
            src[i]     = dst[i];
            sstride[i] = dstride[i];
        }
        stage = stage->next;
    }

    return 0;
}

void avscale_free(AVScaleContext **pctx)
{
    AVScaleContext *ctx;
    AVScaleFilterStage *s, *next;
    int i;

    ctx = *pctx;
    if (!ctx)
        return;

    s = ctx->head;

    while (s) {
        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
            av_freep(&s->dst[i]);
        }
        next = s->next;
        if (s->deinit)
            s->deinit(s);
        av_free(s);
        s = next;
    }
    ctx->head = ctx->tail = 0;

    av_freep(pctx);
}

