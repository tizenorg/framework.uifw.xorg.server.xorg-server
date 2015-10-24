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

#include "hwa_priv.h"
#include <syncsdk.h>
#include <misync.h>
#include <misyncshm.h>
#include <randrstr.h>
#include "hwa.h"

int
hwa_CursorEnable( ScreenPtr screen,  CARD16 *penable)
{
    hwa_screen_priv_ptr        ds = hwa_screen_priv(screen);
    hwa_screen_info_ptr        info = ds->info;
    int                        rc;
  
    if (!info)
        return BadMatch;

    rc = (*info->cursor_enable)(screen, penable);
    if (rc != Success)
        return rc;

    return Success;
}

int
hwa_CursorRotate( ScreenPtr screen, RRCrtc crtc_id, CARD16 *pdegree)
{
    hwa_screen_priv_ptr        ds = hwa_screen_priv(screen);
    hwa_screen_info_ptr        info = ds->info;
    int                        rc;
   
    if (!info)
        return BadMatch;

    rc = (*info->cursor_rotate)(screen, crtc_id, pdegree);
    if (rc != Success)
        return rc;

    return Success;
}

int
hwa_screen_invert_negative(RRCrtcPtr target_crtc, int accessibility_status)
{
    hwa_screen_priv_ptr ds = NULL;
    hwa_screen_info_ptr info = NULL;
    ScreenPtr pScreen = NULL;
    int rc;

    if (!target_crtc)
        return BadValue;

    pScreen = target_crtc->pScreen;
    ds = hwa_screen_priv( pScreen );
    info = ds->info;

    if (!info)
        return BadAlloc;

    rc = info->screen_invert_negative( pScreen, accessibility_status );
    if( rc != Success )
        return rc;

    return Success;
}


int
hwa_screen_scale(RRCrtcPtr target_crtc, int scale_status, int x, int y, int w, int h)
{
    hwa_screen_priv_ptr ds = NULL;
    hwa_screen_info_ptr info = NULL;
    ScreenPtr pScreen = NULL;
    int rc;

    if (!target_crtc)
        return BadValue;

    pScreen = target_crtc->pScreen;
    ds = hwa_screen_priv( pScreen );
    info = ds->info;

    if (!info)
        return BadAlloc;

    rc = info->screen_scale( pScreen, scale_status, x, y, w, h );
    if( rc != Success )
        return rc;

    return Success;
}

int
hwa_ShowLayer( ScreenPtr screen, RRCrtc crtc_id, HWA_OVERLAY_LAYER ovrlayer)
{
    hwa_screen_priv_ptr        ds = hwa_screen_priv(screen);
    hwa_screen_info_ptr        info = ds->info;
    int                        rc;

    if (!info)
        return BadMatch;

    rc = (*info->show_layer)(screen, crtc_id, ovrlayer);
    if (rc != Success)
        return rc;

    return Success;
}

int
hwa_HideLayer( ScreenPtr screen, RRCrtc crtc_id, HWA_OVERLAY_LAYER ovrlayer)
{
    hwa_screen_priv_ptr        ds = hwa_screen_priv(screen);
    hwa_screen_info_ptr        info = ds->info;
    int                        rc;

    if (!info)
        return BadMatch;

    rc = (*info->hide_layer)(screen, crtc_id, ovrlayer);
    if (rc != Success)
        return rc;

    return Success;
}
