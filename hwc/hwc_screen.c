/*
 * Copyright © 2013 SAMSUNG
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Author: Sangjin Lee <lsj119@samsung.com>
 */

#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "hwc_priv.h"

int
hwc_open(ClientPtr client, ScreenPtr screen, RRCrtcPtr crtc, int *maxLayer)
{
    hwc_screen_priv_ptr        ds = hwc_screen_priv(screen);
    hwc_screen_info_ptr        info = ds->info;
    int                         rc;

    if (!info || !(info->open || info->open2))
        return BadMatch;

    if(info->open2)
        rc = (*info->open2) (screen, crtc, maxLayer);
    else
        rc = (*info->open) (screen, maxLayer);
    if (rc != Success)
        return rc;

    return Success;
}

int
hwc_set_drawables(ClientPtr client, ScreenPtr screen, RRCrtcPtr crtc, XID* ids, xRectangle *srcRects, xRectangle *dstRects, HWCCompositeMethod *copmMethods, int count)
{
    hwc_screen_priv_ptr        ds = hwc_screen_priv(screen);
    hwc_screen_info_ptr        info = ds->info;
    int                        rc = BadMatch;
    int i;
    DrawablePtr *drawables;

    if (!info || !(info->open || info->open2))
        return BadMatch;

    drawables = (DrawablePtr *)malloc(sizeof(DrawablePtr)*count);
    for(i = 0; i < count; i++)
    {
        rc  = dixLookupDrawable(drawables+i, ids[i], client, 0, DixReadAccess);
        if (rc != Success)
        {
            free(drawables);
            return rc;
        }
    }
// temp code
#if 0
    for(i=0; i<count ; i++)
    {
        dstRects[i].x = drawables[i]->x;
        dstRects[i].y = drawables[i]->y;
        dstRects[i].width = drawables[i]->width;
        dstRects[i].height = drawables[i]->height;
    }
#endif

    if (info->set_drawables2)
        rc = (*info->set_drawables2) (screen, crtc, drawables, srcRects, dstRects, copmMethods, count);
    else if (info->set_drawables)
        rc = (*info->set_drawables) (screen, drawables, srcRects, dstRects, count);

    if (rc != Success)
    {
        free(drawables);
        return rc;
    }

#ifdef _F_PRESENT_HWC_FLIP_
    /*
     * On this step we want to know which drawables (windows) are owned
     * by HWC extension so we store them in the storage. It is done for
     * HWC and DRI3 cowork
     */

    /*
     * Clear previously stored drawables(windows)
     */
    hwc_storage_clear();

    /*
     * Store new drawables(windows) to the storage
     */
    hwc_storage_add_drawables(drawables, count);
#endif

    free(drawables);

    return Success;
}

