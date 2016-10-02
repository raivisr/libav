/*
 * Copyright (c) 2015 Kostya Shishkov
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.GPLv3.  If not see
 * <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "avstring.h"
#include "buffer.h"
#include "pixformaton.h"
#include "mem.h"
#include "pixdesc.h"
#include "pixfmt.h"

typedef struct AVPixelFormatonRefInternal {
    AVPixelFormaton *pf;
    AVBufferRef *ref;
} AVPixelFormatonRefInternal;

AVPixelFormatonRef *av_pixformaton_alloc(void)
{
    AVPixelFormaton *pf;
    AVBufferRef *buf = NULL;

    AVPixelFormatonRefInternal *pref = NULL;

    buf = av_buffer_allocz(sizeof(*pf));
    if (!buf)
        return NULL;

    pref = av_mallocz(sizeof(*pref));
    if (!pref)
        goto fail;

    pf = (AVPixelFormaton*)buf->data;

    pref->pf  = pf;
    pref->ref = buf;

    return (AVPixelFormatonRef*)pref;
fail:
    av_buffer_unref(&buf);
    av_freep(&pref);
    return NULL;
}

AVPixelFormatonRef *av_pixformaton_ref(AVPixelFormatonRef *pf)
{
    AVPixelFormatonRefInternal *src = (AVPixelFormatonRefInternal*)pf;
    AVPixelFormatonRefInternal *dst;

    dst = av_mallocz(sizeof(*dst));
    if (!dst)
        return NULL;

    dst->ref = av_buffer_ref(src->ref);
    if (!dst->ref) {
        av_freep(&dst);
        return NULL;
    }

    dst->pf = src->pf;

    return (AVPixelFormatonRef*)dst;
}

void av_pixformaton_unref(AVPixelFormatonRef **pref)
{
    AVPixelFormatonRefInternal *pref_int = (AVPixelFormatonRefInternal*)*pref;

    if (!pref_int)
        return;

    av_buffer_unref(&pref_int->ref);
    av_freep(&pref_int);
}

AVPixelFormatonRef *av_pixformaton_from_pixfmt(enum AVPixelFormat pix_fmt)
{
    AVPixelFormaton *pf;
    AVPixelFormatonRef *pref;
    const AVPixFmtDescriptor *desc;

    int i;

    desc = av_pix_fmt_desc_get(pix_fmt);
    if (!desc)
        return NULL;

    pref = av_pixformaton_alloc();
    if (!pref)
        return NULL;

    pf = pref->pf;

    if (desc->flags & AV_PIX_FMT_FLAG_BE)
        pf->flags |= AV_PIX_FORMATON_FLAG_BE;
    if (desc->flags & AV_PIX_FMT_FLAG_ALPHA)
        pf->flags |= AV_PIX_FORMATON_FLAG_ALPHA;
    if (desc->flags & AV_PIX_FMT_FLAG_PAL)
        pf->flags |= AV_PIX_FORMATON_FLAG_PAL;

    if (desc->flags & AV_PIX_FMT_FLAG_RGB)
        pf->model = AVCOL_MODEL_RGB;
    else
        pf->model = AVCOL_MODEL_YUV;

#if 0
    if (av_strstart(desc->name, "yuvj", NULL))
        pf->range = AVCOL_RANGE_JPEG;
    else
        pf->range = AVCOL_RANGE_UNSPECIFIED;

    pf->primaries = AVCOL_PRI_UNSPECIFIED;
    pf->transfer  = AVCOL_TRC_UNSPECIFIED;
    pf->space     = AVCOL_SPC_UNSPECIFIED;
    pf->location  = AVCHROMA_LOC_UNSPECIFIED;
#endif

    pf->nb_components = desc->nb_components;

    if (!(desc->flags & AV_PIX_FMT_FLAG_PLANAR))
        pf->pixel_size = (desc->comp[0].depth * desc->nb_components + 7) >> 3;

    for (i = 0; i < pf->nb_components; i++) {
        AVPixelChromaton *chromaton = &pf->component[i];
        const AVComponentDescriptor *comp = &desc->comp[i];

        chromaton->plane     = comp->plane;
        chromaton->next      = comp->step;
        chromaton->h_sub = i > 0 && i < 3 ? desc->log2_chroma_w : 0;
        chromaton->v_sub = i > 0 && i < 3 ? desc->log2_chroma_h : 0;
        chromaton->offset    = comp->offset;
        chromaton->shift     = comp->shift;
        chromaton->depth     = comp->depth;
        chromaton->packed    = !(desc->flags & AV_PIX_FMT_FLAG_PLANAR);
    }

    return pref;
}
