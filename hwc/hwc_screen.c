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
hwc_open(ClientPtr client, ScreenPtr screen, int *maxLayer)
{
    hwc_screen_priv_ptr        ds = hwc_screen_priv(screen);
    hwc_screen_info_ptr        info = ds->info;
    int                         rc;

    if (!info || !info->open)
        return BadMatch;

    rc = (*info->open) (screen, maxLayer);
    if (rc != Success)
        return rc;

    return Success;
}

int
hwc_set_drawables(ClientPtr client, ScreenPtr screen, XID* ids, int count)
{
    hwc_screen_priv_ptr        ds = hwc_screen_priv(screen);
    hwc_screen_info_ptr        info = ds->info;
    int                         rc;
    int i;
    DrawablePtr *drawables;

    if (!info || !info->open)
        return BadMatch;

    if (count > info->maxLayer)
        return BadMatch;
        
    drawables = (DrawablePtr *)malloc(sizeof(DrawablePtr)*count);
    for(i=0; i < count; i++)
    {
        rc  = dixLookupDrawable(drawables+i, ids[i], client, 0, DixReadAccess);
        if (rc != Success)
            return rc;
    }

    rc = (*info->set_drawables)(screen, drawables, count);
    if (rc != Success)
        return rc;

    return Success;
}

