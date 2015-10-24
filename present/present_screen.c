/*
 * Copyright © 2013 Keith Packard
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
 */

#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "present_priv.h"

#ifdef _F_PRESENT_SELECTIVE_COMPOSITE_
#include <X11/Xatom.h>
#endif //_F_PRESENT_SELECTIVE_COMPOSITE_

int present_request;
DevPrivateKeyRec present_screen_private_key;
DevPrivateKeyRec present_window_private_key;

#if defined (_F_PRESENT_PIXMAP_SWAP_) || defined (_F_PRESENT_HWC_FLIP_)
struct xorg_list present_name_pixmap_list;
#endif //_F_PRESENT_PIXMAP_SWAP_

#ifdef _F_PRESENT_SELECTIVE_COMPOSITE_
static void
_present_set_selective_composite (WindowPtr window)
{
    static Atom atom_use_dri2= 0;
    static int use = 1;

    if(!atom_use_dri2)
        atom_use_dri2 = MakeAtom ("X_WIN_USE_DRI2", 14, TRUE);

    dixChangeWindowProperty (serverClient,
                             window, atom_use_dri2, XA_CARDINAL, 32,
                             PropModeReplace, 1, &use, TRUE);
}
#endif //_F_PRESENT_SELECTIVE_COMPOSITE_

/*
 * Get a pointer to a present window private, creating if necessary
 */
present_window_priv_ptr
present_get_window_priv(WindowPtr window, Bool create)
{
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (!create || window_priv != NULL)
        return window_priv;
    window_priv = calloc (1, sizeof (present_window_priv_rec));
    if (!window_priv)
        return NULL;
    xorg_list_init(&window_priv->vblank);
    xorg_list_init(&window_priv->notifies);
#ifdef _F_PRESENT_SELECTIVE_COMPOSITE_
    _present_set_selective_composite (window);
#endif //_F_PRESENT_SELECTIVE_COMPOSITE_
    dixSetPrivate(&window->devPrivates, &present_window_private_key, window_priv);
    return window_priv;
}

/*
 * Hook the close screen function to clean up our screen private
 */
static Bool
present_close_screen(ScreenPtr screen)
{
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);

    present_flip_destroy(screen);

    unwrap(screen_priv, screen, CloseScreen);
#if defined (_F_PRESENT_PIXMAP_SWAP_) || defined (_F_PRESENT_HWC_FLIP_)
    unwrap(screen_priv, screen, NameWindowPixmap);
    unwrap(screen_priv, screen, ProcCompRedirectWindow);
    unwrap(screen_priv, screen, ProcCompUnredirectWindow);
    unwrap(screen_priv, screen, DestroyPixmap);
#endif //_F_PRESENT_PIXMAP_SWAP_
    (*screen->CloseScreen) (screen);
    free(screen_priv);
    return TRUE;
}

/*
 * Free any queued presentations for this window
 */
static void
present_free_window_vblank(WindowPtr window)
{
    present_window_priv_ptr     window_priv = present_window_priv(window);
    present_vblank_ptr          vblank, tmp;

    xorg_list_for_each_entry_safe(vblank, tmp, &window_priv->vblank, window_list) {
        present_abort_vblank(window->drawable.pScreen, vblank->crtc, vblank->event_id, vblank->target_msc);
        present_vblank_destroy(vblank);
    }
#ifdef _F_PRESENT_PIXMAP_SWAP_
    if (window_priv->swap_vblank) {
        present_pixmap_idle(window_priv->swap_vblank->pixmap,
                            window_priv->swap_vblank->window,
                            window_priv->swap_vblank->serial,
                            window_priv->swap_vblank->idle_fence);
        present_vblank_destroy(window_priv->swap_vblank);
    }
#endif //_F_PRESENT_PIXMAP_SWAP_
}

/*
 * Clean up any pending or current flips for this window
 */
static void
present_clear_window_flip(WindowPtr window)
{
    ScreenPtr                   screen = window->drawable.pScreen;
    present_screen_priv_ptr     screen_priv = present_screen_priv(screen);
    present_vblank_ptr          flip_pending = screen_priv->flip_pending;

    if (flip_pending && flip_pending->window == window) {
#ifndef _F_PRESENT_PIXMAP_SWAP_
        assert (flip_pending->abort_flip);
#endif
        flip_pending->window = NULL;
    }
    if (screen_priv->flip_window == window)
        screen_priv->flip_window = NULL;

#ifdef _F_PRESENT_HWC_FLIP_
    present_window_flip_idle(window);
#endif //_F_PRESENT_HWC_FLIP_
}

/*
 * Hook the close window function to clean up our window private
 */
static Bool
present_destroy_window(WindowPtr window)
{
    Bool ret;
    ScreenPtr screen = window->drawable.pScreen;
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (window_priv) {
        present_clear_window_notifies(window);
        present_free_events(window);
        present_free_window_vblank(window);
        present_clear_window_flip(window);

        free(window_priv);
    }
    unwrap(screen_priv, screen, DestroyWindow);
    if (screen->DestroyWindow)
        ret = screen->DestroyWindow (window);
    else
        ret = TRUE;
    wrap(screen_priv, screen, DestroyWindow, present_destroy_window);
    return ret;
}

/*
 * Hook the config notify screen function to deliver present config notify events
 */
static int
present_config_notify(WindowPtr window,
                   int x, int y, int w, int h, int bw,
                   WindowPtr sibling)
{
    int ret;
    ScreenPtr screen = window->drawable.pScreen;
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);

    present_send_config_notify(window, x, y, w, h, bw, sibling);

    unwrap(screen_priv, screen, ConfigNotify);
    if (screen->ConfigNotify)
        ret = screen->ConfigNotify (window, x, y, w, h, bw, sibling);
    else
        ret = 0;
    wrap(screen_priv, screen, ConfigNotify, present_config_notify);
    return ret;
}

/*
 * Hook the clip notify screen function to un-flip as necessary
 */

static void
present_clip_notify(WindowPtr window, int dx, int dy)
{
    ScreenPtr screen = window->drawable.pScreen;
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);

    present_check_flip_window(window);
    unwrap(screen_priv, screen, ClipNotify)
    if (screen->ClipNotify)
        screen->ClipNotify (window, dx, dy);
    wrap(screen_priv, screen, ClipNotify, present_clip_notify);
}

#if defined (_F_PRESENT_PIXMAP_SWAP_) || defined (_F_PRESENT_HWC_FLIP_)
int
present_name_window_pixmap (WindowPtr pWin, PixmapPtr pPix, CARD32 namePix)
{
    int ret;
    ScreenPtr                   screen = pWin->drawable.pScreen;
    present_screen_priv_ptr     screen_priv = present_screen_priv(screen);
    present_window_priv_ptr     window_priv = present_window_priv(pWin);
    present_name_pixmap_ptr     name_pixmap;

    name_pixmap = calloc (1, sizeof (present_name_pixmap_rec));
    if (!name_pixmap)
        return BadAlloc;

    name_pixmap->id = namePix;
    name_pixmap->pixmap = pPix;
    name_pixmap->window = pWin;

    xorg_list_append(&name_pixmap->name_pixmap_list, &present_name_pixmap_list);

    DebugPixmap("  add name pixmap list id:%p pix_id:%p, pixmap:%p (%dx%d) refcnt:%d pWin:%p pWin_id:%p",
        name_pixmap->id,
        name_pixmap->pixmap->drawable.id,
        name_pixmap->pixmap,
        name_pixmap->pixmap->drawable.width,
        name_pixmap->pixmap->drawable.height,
        name_pixmap->pixmap->refcnt,
        name_pixmap->window,
        name_pixmap->window->drawable.id);

    unwrap(screen_priv, screen, NameWindowPixmap)
    if (screen->NameWindowPixmap)
        ret = screen->NameWindowPixmap (pWin, pPix, namePix);
    else
        ret = 0;
    wrap(screen_priv, screen, NameWindowPixmap, present_name_window_pixmap);

    return ret ;
}

static int
present_proc_comp_redirect_window (WindowPtr window, int update)
{
    ScreenPtr                   screen = window->drawable.pScreen;
    present_screen_priv_ptr     screen_priv = present_screen_priv(screen);
    int ret;
    CARD16 scanout = 0;

    present_send_tree_scanout_notify (window, scanout);

    unwrap(screen_priv, screen, ProcCompRedirectWindow);
    if (screen->ProcCompRedirectWindow)
        ret = screen->ProcCompRedirectWindow (window, update);
    else
        ret = 0;
    wrap(screen_priv, screen, ProcCompRedirectWindow, present_proc_comp_redirect_window);

    return ret;
}

static int
present_proc_comp_unredirect_window (WindowPtr window, int update)
{
    ScreenPtr                   screen = window->drawable.pScreen;
    present_screen_priv_ptr     screen_priv = present_screen_priv(screen);
    int ret;
    CARD16 scanout = 1;

    present_send_tree_scanout_notify (window, scanout);

    unwrap(screen_priv, screen, ProcCompUnredirectWindow);
    if (screen->ProcCompUnredirectWindow)
        ret = screen->ProcCompUnredirectWindow (window, update);
    else
        ret = 0;
    wrap(screen_priv, screen, ProcCompUnredirectWindow, present_proc_comp_unredirect_window);

    return ret;
}

static int
present_destroy_pixmap (PixmapPtr pixmap)
{
    ScreenPtr                   screen = pixmap->drawable.pScreen;
    present_screen_priv_ptr     screen_priv = present_screen_priv(screen);
    int ret;
    present_name_pixmap_ptr     name_pixmap, temp;

    xorg_list_for_each_entry_safe(name_pixmap, temp,    &present_name_pixmap_list, name_pixmap_list) {

        if (pixmap->destroy_pixmap_id == name_pixmap->id) {
            DebugPixmap("     remove name pixmap list id:%p pix_id:%p, pixmap:%p (%dx%d) refcnt:%d pWin:%p pWin_id:%p",
                name_pixmap->id,
                pixmap->drawable.id,
                pixmap,
                pixmap->drawable.width,
                pixmap->drawable.height,
                pixmap->refcnt,
                name_pixmap->window,
                name_pixmap->window->drawable.id);

            xorg_list_del (&name_pixmap->name_pixmap_list);
            free (name_pixmap);
        }
    }

    unwrap(screen_priv, screen, DestroyPixmap);
    if (screen->DestroyPixmap)
        ret = screen->DestroyPixmap (pixmap);
    else
        ret = 0;
    wrap(screen_priv, screen, DestroyPixmap, present_destroy_pixmap);

    return ret;
}
#endif //_F_PRESENT_PIXMAP_SWAP_

/*
 * Initialize a screen for use with present
 */
int
present_screen_init(ScreenPtr screen, present_screen_info_ptr info)
{
    if (!dixRegisterPrivateKey(&present_screen_private_key, PRIVATE_SCREEN, 0))
        return FALSE;

    if (!dixRegisterPrivateKey(&present_window_private_key, PRIVATE_WINDOW, 0))
        return FALSE;

    if (!present_screen_priv(screen)) {
        present_screen_priv_ptr screen_priv = calloc(1, sizeof (present_screen_priv_rec));
        if (!screen_priv)
            return FALSE;

        wrap(screen_priv, screen, CloseScreen, present_close_screen);
        wrap(screen_priv, screen, DestroyWindow, present_destroy_window);
        wrap(screen_priv, screen, ConfigNotify, present_config_notify);
        wrap(screen_priv, screen, ClipNotify, present_clip_notify);

#if defined (_F_PRESENT_PIXMAP_SWAP_) || defined (_F_PRESENT_HWC_FLIP_)
        wrap(screen_priv, screen, NameWindowPixmap, present_name_window_pixmap);
        wrap(screen_priv, screen, ProcCompRedirectWindow, present_proc_comp_redirect_window);
        wrap(screen_priv, screen, ProcCompUnredirectWindow, present_proc_comp_unredirect_window);
        wrap(screen_priv, screen, DestroyPixmap, present_destroy_pixmap);

        xorg_list_init(&present_name_pixmap_list);
#endif //_F_PRESENT_PIXMAP_SWAP_

        screen_priv->info = info;

        dixSetPrivate(&screen->devPrivates, &present_screen_private_key, screen_priv);

        present_fake_screen_init(screen);
    }

    return TRUE;
}

/*
 * Initialize the present extension
 */
void
present_extension_init(void)
{
    ExtensionEntry *extension;
    int i;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
        return;
#endif

    extension = AddExtension(PRESENT_NAME, PresentNumberEvents, PresentNumberErrors,
                             proc_present_dispatch, sproc_present_dispatch,
                             NULL, StandardMinorOpcode);
    if (!extension)
        goto bail;

    present_request = extension->base;

    if (!present_init())
        goto bail;

    if (!present_event_init())
        goto bail;

    for (i = 0; i < screenInfo.numScreens; i++) {
        if (!present_screen_init(screenInfo.screens[i], NULL))
            goto bail;
    }
    return;

bail:
    FatalError("Cannot initialize Present extension");
}
