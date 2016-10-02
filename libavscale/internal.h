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

#ifndef AVSCALE_INTERNAL_H
#define AVSCALE_INTERNAL_H

#include "libavutil/dict.h"
#include "libavutil/log.h"
#include "libavutil/pixformaton.h"

#include "avscale.h"

#define AVSCALE_MAX_COMPONENTS AV_PIX_FORMATON_COMPONENTS

typedef struct AVScaleFilterStage {
    void (*deinit)(struct AVScaleFilterStage *stage);
    void (*do_common)(void *ctx,
                      uint8_t *src[AVSCALE_MAX_COMPONENTS],
                      int sstrides[AVSCALE_MAX_COMPONENTS],
                      uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                      int dstrides[AVSCALE_MAX_COMPONENTS],
                      int w, int h);
    void (*do_component[AVSCALE_MAX_COMPONENTS])(void *ctx,
                                                 uint8_t *src, int sstride,
                                                 uint8_t *dst, int dstride,
                                                 int w, int h);

    void *do_common_ctx;
    void *do_component_ctx[AVSCALE_MAX_COMPONENTS];

    uint8_t *src[AVSCALE_MAX_COMPONENTS]; // null if current input should be used
    int      src_stride[AVSCALE_MAX_COMPONENTS];
    uint8_t *dst[AVSCALE_MAX_COMPONENTS]; // null if default output should be used
    int      dst_stride[AVSCALE_MAX_COMPONENTS];
    int      w[AVSCALE_MAX_COMPONENTS], h[AVSCALE_MAX_COMPONENTS];

    struct AVScaleFilterStage *next;
} AVScaleFilterStage;

struct AVScaleContext {
    const AVClass *av_class;
    const AVPixelFormaton *src_fmt, *dst_fmt, *cur_fmt;
    int cur_w, cur_h;
    int dst_w, dst_h;
    AVScaleFilterStage *head, *tail;
};

typedef struct AVScaleKernel {
    const char *name; // for convenience

    // init worker-specific contexts and set working functions
    // e.g. for YUV2RGB it will init do_common_ctx with YUV2RGB tables
    // and set do_common = convert_yuv2rgb
    int (*kernel_init)(AVScaleContext *ctx,
                       const struct AVScaleKernel *kern,
                       AVScaleFilterStage *stage,
                       AVDictionary *opts);
} AVScaleKernel;

const AVScaleKernel *avscale_find_kernel(const char *name);
int avscale_apply_kernel(AVScaleContext *ctx,
                         const char *name,
                         AVScaleFilterStage *stage);

uint8_t *avscale_get_component_ptr(const AVFrame *src, int component_id);
int avscale_get_component_stride(const AVFrame *src, int component_id);


#endif /* AVSCALE_INTERNAL_H */
