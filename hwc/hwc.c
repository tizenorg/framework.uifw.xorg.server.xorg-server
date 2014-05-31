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

int hwc_request;
DevPrivateKeyRec hwc_screen_private_key;
DevPrivateKeyRec hwc_window_private_key;

static Bool
hwc_close_screen(ScreenPtr screen)
{
    hwc_screen_priv_ptr screen_priv = hwc_screen_priv(screen);

    unwrap(screen_priv, screen, CloseScreen);

    free(screen_priv);
    return (*screen->CloseScreen) (screen);
}

static int 
hwc_config_notify (WindowPtr pWin ,
                                    int x ,
                                    int y ,
                                    int w ,
                                    int h ,
                                    int bw ,
                                    WindowPtr pSib )
{
    DrawablePtr drawable = (DrawablePtr) pWin;
    ScreenPtr screen = drawable->pScreen;
    hwc_screen_priv_ptr screen_priv = hwc_screen_priv(screen);
    int ret;

    unwrap(screen_priv, screen, ConfigNotify);
    ret = (*screen->ConfigNotify) (pWin, x, y, w, h, bw, pSib);
    wrap(screen_priv, screen, ConfigNotify, hwc_config_notify);

    
    /* TODO */
    
    return ret;
}

static int 
hwc_destroy_window (WindowPtr pWin)
{
    DrawablePtr drawable = (DrawablePtr) pWin;
    ScreenPtr screen = drawable->pScreen;
    hwc_screen_priv_ptr screen_priv = hwc_screen_priv(screen);
    int ret;

    hwc_free_events(pWin);
    
    unwrap(screen_priv, screen, DestroyWindow);
    ret = (*screen->DestroyWindow) (pWin);
    wrap(screen_priv, screen, DestroyWindow, hwc_destroy_window);

    return ret;
}


Bool
hwc_screen_init(ScreenPtr screen, hwc_screen_info_ptr info)
{
    if (!dixRegisterPrivateKey(&hwc_screen_private_key, PRIVATE_SCREEN, 0))
        return FALSE;

    if (!hwc_screen_priv(screen)) {
        hwc_screen_priv_ptr screen_priv = calloc(1, sizeof (hwc_screen_priv_rec));
        if (!screen_priv)
            return FALSE;

        wrap(screen_priv, screen, CloseScreen, hwc_close_screen);
        wrap(screen_priv, screen, ConfigNotify, hwc_config_notify);
        wrap(screen_priv, screen, DestroyWindow, hwc_destroy_window);

        screen_priv->info = info;

        dixSetPrivate(&screen->devPrivates, &hwc_screen_private_key, screen_priv);
    }

    return TRUE;
}

void
hwc_extension_init(void)
{
    ExtensionEntry *extension;
    int i;

    extension = AddExtension(HWC_NAME, HWCNumberEvents, HWCNumberErrors,
                             proc_hwc_dispatch, sproc_hwc_dispatch,
                             NULL, StandardMinorOpcode);
    if (!extension)
        goto bail;

    hwc_request = extension->base;

    if (!hwc_event_init())
        goto bail;
        
    for (i = 0; i < screenInfo.numScreens; i++) {
        if (!hwc_screen_init(screenInfo.screens[i], NULL))
            goto bail;
    }
    return;

bail:
    FatalError("Cannot initialize HWC extension");
}
