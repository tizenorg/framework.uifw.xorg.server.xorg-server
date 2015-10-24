/*
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "compint.h"

#ifdef PANORAMIX
#include "panoramiXsrv.h"
#endif

#ifdef _F_STEREOSCOPIC_LEFT_BUFFER_COODINATE_
static int prefered_w = 0 ;
static int prefered_h = 0 ;

#define IS_NOT_SET 0
#define IS_SET 1
#endif  // _F_STEREOSCOPIC_LEFT_BUFFER_COODINATE_

#ifdef ENABLE_DLOG
#define LOG_TAG "XORG_PIXMAP"
#include <dlog.h>
#endif

#ifdef COMPOSITE_DEBUG
static int
compCheckWindow(WindowPtr pWin, void *data)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    PixmapPtr pWinPixmap = (*pScreen->GetWindowPixmap) (pWin);
    PixmapPtr pParentPixmap =
        pWin->parent ? (*pScreen->GetWindowPixmap) (pWin->parent) : 0;
    PixmapPtr pScreenPixmap = (*pScreen->GetScreenPixmap) (pScreen);

    if (!pWin->parent) {
        assert(pWin->redirectDraw == RedirectDrawNone);
        assert(pWinPixmap == pScreenPixmap);
    }
    else if (pWin->redirectDraw != RedirectDrawNone) {
        assert(pWinPixmap != pParentPixmap);
        assert(pWinPixmap != pScreenPixmap);
    }
    else {
        assert(pWinPixmap == pParentPixmap);
    }
    assert(0 < pWinPixmap->refcnt && pWinPixmap->refcnt < 3);
    assert(0 < pScreenPixmap->refcnt && pScreenPixmap->refcnt < 3);
    if (pParentPixmap)
        assert(0 <= pParentPixmap->refcnt && pParentPixmap->refcnt < 3);
    return WT_WALKCHILDREN;
}

void
compCheckTree(ScreenPtr pScreen)
{
    WalkTree(pScreen, compCheckWindow, 0);
}
#endif

typedef struct _compPixmapVisit {
    WindowPtr pWindow;
    PixmapPtr pPixmap;
} CompPixmapVisitRec, *CompPixmapVisitPtr;

static Bool
compRepaintBorder(ClientPtr pClient, void *closure)
{
    WindowPtr pWindow;
    int rc =
        dixLookupWindow(&pWindow, (XID) (intptr_t) closure, pClient,
                        DixWriteAccess);

    if (rc == Success) {
        RegionRec exposed;

        RegionNull(&exposed);
        RegionSubtract(&exposed, &pWindow->borderClip, &pWindow->winSize);
        miPaintWindow(pWindow, &exposed, PW_BORDER);
        RegionUninit(&exposed);
    }
    return TRUE;
}

static int
compSetPixmapVisitWindow(WindowPtr pWindow, void *data)
{
    CompPixmapVisitPtr pVisit = (CompPixmapVisitPtr) data;
    ScreenPtr pScreen = pWindow->drawable.pScreen;

    if (pWindow != pVisit->pWindow && pWindow->redirectDraw != RedirectDrawNone)
        return WT_DONTWALKCHILDREN;
    (*pScreen->SetWindowPixmap) (pWindow, pVisit->pPixmap);
    /*
     * Recompute winSize and borderSize.  This is duplicate effort
     * when resizing pixmaps, but necessary when changing redirection.
     * Might be nice to fix this.
     */
    SetWinSize(pWindow);
    SetBorderSize(pWindow);
    if (HasBorder(pWindow))
        QueueWorkProc(compRepaintBorder, serverClient,
                      (void *) (intptr_t) pWindow->drawable.id);
    return WT_WALKCHILDREN;
}

void
compSetPixmap(WindowPtr pWindow, PixmapPtr pPixmap)
{
    CompPixmapVisitRec visitRec;

    visitRec.pWindow = pWindow;
    visitRec.pPixmap = pPixmap;
    TraverseTree(pWindow, compSetPixmapVisitWindow, (void *) &visitRec);
    compCheckTree(pWindow->drawable.pScreen);
}

Bool
compCheckRedirect(WindowPtr pWin)
{
    CompWindowPtr cw = GetCompWindow(pWin);
    CompScreenPtr cs = GetCompScreen(pWin->drawable.pScreen);
    Bool should;
    ScreenPtr pScreen = NULL;
    PixmapPtr pPixmap = NULL;
    Bool ret = FALSE;

    should = pWin->realized && (pWin->drawable.class != InputOnly) &&
        (cw != NULL) && (pWin->parent != NULL);

    /* Never redirect the overlay window */
    if (cs->pOverlayWin != NULL) {
        if (pWin == cs->pOverlayWin) {
            should = FALSE;
        }
    }

    if (should != (pWin->redirectDraw != RedirectDrawNone)) {
        if (should) {
            ret = compAllocPixmap(pWin);
            pScreen = pWin->drawable.pScreen;
            pPixmap = (*pScreen->GetWindowPixmap) (pWin);

#ifdef ENABLE_DLOG
            SLOG(LOG_INFO, "XORG_PIXMAP", "pix_id:%p, pixmap:%p (%dx%d) refcnt:%d win_id:%p win:%p (%dx%d) view:%d\n",
                pPixmap->drawable.id,
                pPixmap,
                pPixmap->drawable.width,
                pPixmap->drawable.height,
                pPixmap->refcnt,
                pWin->drawable.id,
                pWin,
                pWin->drawable.width,
                pWin->drawable.height,
                pWin->viewable);
#endif

            return ret;
        }
        else {
            pScreen = pWin->drawable.pScreen;
            pPixmap = (*pScreen->GetWindowPixmap) (pWin);

            compSetParentPixmap(pWin);
            compRestoreWindow(pWin, pPixmap);

#ifdef ENABLE_DLOG
            SLOG(LOG_INFO, "XORG_PIXMAP", "pix_id:%p, pixmap:%p (%dx%d) refcnt:%d-1 win_id:%p win:%p (%dx%d) view:%d\n",
                pPixmap->drawable.id,
                pPixmap,
                pPixmap->drawable.width,
                pPixmap->drawable.height,
                pPixmap->refcnt,
                pWin->drawable.id,
                pWin,
                pWin->drawable.width,
                pWin->drawable.height,
                pWin->viewable);

#endif
            (*pScreen->DestroyPixmap) (pPixmap);
        }
    }
    else if (should) {
        if (cw->update == CompositeRedirectAutomatic)
            pWin->redirectDraw = RedirectDrawAutomatic;
        else
            pWin->redirectDraw = RedirectDrawManual;
    }

#ifdef ENABLE_DLOG
    if (!pPixmap) {
        pScreen = pWin->drawable.pScreen;
        pPixmap = (*pScreen->GetWindowPixmap) (pWin);
    }

    SLOG(LOG_INFO, "XORG_PIXMAP", "pix_id:%p, pixmap:%p (%dx%d) refcnt:%d win_id:%p win:%p (%dx%d) view:%d\n",
        pPixmap->drawable.id,
        pPixmap,
        pPixmap->drawable.width,
        pPixmap->drawable.height,
        pPixmap->refcnt,
        pWin->drawable.id,
        pWin,
        pWin->drawable.width,
        pWin->drawable.height,
        pWin->viewable);
#endif

    return TRUE;
}

static int
updateOverlayWindow(ScreenPtr pScreen)
{
    CompScreenPtr cs;
    WindowPtr pWin;             /* overlay window */
    XID vlist[2];
    int w = pScreen->width;
    int h = pScreen->height;

#ifdef PANORAMIX
    if (!noPanoramiXExtension) {
        w = PanoramiXPixWidth;
        h = PanoramiXPixHeight;
    }
#endif

    cs = GetCompScreen(pScreen);
    if ((pWin = cs->pOverlayWin) != NULL) {
        if ((pWin->drawable.width == w) && (pWin->drawable.height == h))
            return Success;

        /* Let's resize the overlay window. */
        vlist[0] = w;
        vlist[1] = h;
        return ConfigureWindow(pWin, CWWidth | CWHeight, vlist, wClient(pWin));
    }

    /* Let's be on the safe side and not assume an overlay window is
       always allocated. */
    return Success;
}

Bool
compPositionWindow(WindowPtr pWin, int x, int y)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    Bool ret = TRUE;

    pScreen->PositionWindow = cs->PositionWindow;
    /*
     * "Shouldn't need this as all possible places should be wrapped
     *
     compCheckRedirect (pWin);
     */
#ifdef COMPOSITE_DEBUG
    if ((pWin->redirectDraw != RedirectDrawNone) !=
        (pWin->viewable && (GetCompWindow(pWin) != NULL)))
        OsAbort();
#endif
    if (pWin->redirectDraw != RedirectDrawNone) {
        PixmapPtr pPixmap = (*pScreen->GetWindowPixmap) (pWin);
        int bw = wBorderWidth(pWin);
        int nx = pWin->drawable.x - bw;
        int ny = pWin->drawable.y - bw;

        if (pPixmap->screen_x != nx || pPixmap->screen_y != ny) {
            pPixmap->screen_x = nx;
            pPixmap->screen_y = ny;
            pPixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;
        }
    }

    if (!(*pScreen->PositionWindow) (pWin, x, y))
        ret = FALSE;
    cs->PositionWindow = pScreen->PositionWindow;
    pScreen->PositionWindow = compPositionWindow;
    compCheckTree(pWin->drawable.pScreen);
    if (updateOverlayWindow(pScreen) != Success)
        ret = FALSE;
    return ret;
}

Bool
compRealizeWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    Bool ret = TRUE;

    pScreen->RealizeWindow = cs->RealizeWindow;
    compCheckRedirect(pWin);
    if (!(*pScreen->RealizeWindow) (pWin))
        ret = FALSE;
    cs->RealizeWindow = pScreen->RealizeWindow;
    pScreen->RealizeWindow = compRealizeWindow;
    compCheckTree(pWin->drawable.pScreen);
    return ret;
}

Bool
compUnrealizeWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    Bool ret = TRUE;

    pScreen->UnrealizeWindow = cs->UnrealizeWindow;
    compCheckRedirect(pWin);
    if (!(*pScreen->UnrealizeWindow) (pWin))
        ret = FALSE;
    cs->UnrealizeWindow = pScreen->UnrealizeWindow;
    pScreen->UnrealizeWindow = compUnrealizeWindow;
    compCheckTree(pWin->drawable.pScreen);
    return ret;
}

/*
 * Called after the borderClip for the window has settled down
 * We use this to make sure our extra borderClip has the right origin
 */

void
compClipNotify(WindowPtr pWin, int dx, int dy)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    CompWindowPtr cw = GetCompWindow(pWin);

    if (cw) {
        if (cw->borderClipX != pWin->drawable.x ||
            cw->borderClipY != pWin->drawable.y) {
            RegionTranslate(&cw->borderClip,
                            pWin->drawable.x - cw->borderClipX,
                            pWin->drawable.y - cw->borderClipY);
            cw->borderClipX = pWin->drawable.x;
            cw->borderClipY = pWin->drawable.y;
        }
    }
    if (cs->ClipNotify) {
        pScreen->ClipNotify = cs->ClipNotify;
        (*pScreen->ClipNotify) (pWin, dx, dy);
        cs->ClipNotify = pScreen->ClipNotify;
        pScreen->ClipNotify = compClipNotify;
    }
}

/*
 * Returns TRUE if the window needs server-provided automatic redirect,
 * which is true if the child and parent aren't both regular or ARGB visuals
 */

static Bool
compIsAlternateVisual(ScreenPtr pScreen, XID visual)
{
    CompScreenPtr cs = GetCompScreen(pScreen);
    int i;

    for (i = 0; i < cs->numAlternateVisuals; i++)
        if (cs->alternateVisuals[i] == visual)
            return TRUE;
    return FALSE;
}

static Bool
compIsImplicitRedirectException(ScreenPtr pScreen,
                                XID parentVisual, XID winVisual)
{
    CompScreenPtr cs = GetCompScreen(pScreen);
    int i;

    for (i = 0; i < cs->numImplicitRedirectExceptions; i++)
        if (cs->implicitRedirectExceptions[i].parentVisual == parentVisual &&
            cs->implicitRedirectExceptions[i].winVisual == winVisual)
            return TRUE;

    return FALSE;
}

static Bool
compImplicitRedirect(WindowPtr pWin, WindowPtr pParent)
{
#ifdef _F_NO_IMPLICIT_REDIRECT_
    /*
    Xserver redirect windows inside when the visuals(ex.depth) between parent of a window and the window is different.
    Do not check the visuals between them in the composite extension when xserver redirect a window.
    This make the 32 depth window get the root pixmap when compositor unredirect the 32 depth toplevel window.
    */
    return FALSE;
#endif
    if (pParent) {
        ScreenPtr pScreen = pWin->drawable.pScreen;
        XID winVisual = wVisual(pWin);
        XID parentVisual = wVisual(pParent);

        if (compIsImplicitRedirectException(pScreen, parentVisual, winVisual))
            return FALSE;

        if (winVisual != parentVisual &&
            (compIsAlternateVisual(pScreen, winVisual) ||
             compIsAlternateVisual(pScreen, parentVisual)))
            return TRUE;
    }
    return FALSE;
}

static void
compFreeOldPixmap(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;

    if (pWin->redirectDraw != RedirectDrawNone) {
        CompWindowPtr cw = GetCompWindow(pWin);

        if (cw->pOldPixmap) {
#ifdef ENABLE_DLOG
            SLOG(LOG_INFO, "XORG_PIXMAP", "pix_id:%p, pixmap:%p (%dx%d) refcnt:%d-1 win_id:%p win:%p (%dx%d) view:%d\n",
                cw->pOldPixmap->drawable.id,
                cw->pOldPixmap,
                cw->pOldPixmap->drawable.width,
                cw->pOldPixmap->drawable.height,
                cw->pOldPixmap->refcnt,
                pWin->drawable.id,
                pWin,
                pWin->drawable.width,
                pWin->drawable.height,
                pWin->viewable);

#endif
            (*pScreen->DestroyPixmap) (cw->pOldPixmap);
            cw->pOldPixmap = NullPixmap;
        }
    }
}

void
compMoveWindow(WindowPtr pWin, int x, int y, WindowPtr pSib, VTKind kind)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);

    pScreen->MoveWindow = cs->MoveWindow;
    (*pScreen->MoveWindow) (pWin, x, y, pSib, kind);
    cs->MoveWindow = pScreen->MoveWindow;
    pScreen->MoveWindow = compMoveWindow;

    compFreeOldPixmap(pWin);
    compCheckTree(pScreen);
}

void
compResizeWindow(WindowPtr pWin, int x, int y,
                 unsigned int w, unsigned int h, WindowPtr pSib)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);

    pScreen->ResizeWindow = cs->ResizeWindow;
    (*pScreen->ResizeWindow) (pWin, x, y, w, h, pSib);
    cs->ResizeWindow = pScreen->ResizeWindow;
    pScreen->ResizeWindow = compResizeWindow;

    compFreeOldPixmap(pWin);
    compCheckTree(pWin->drawable.pScreen);
}

void
compChangeBorderWidth(WindowPtr pWin, unsigned int bw)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);

    pScreen->ChangeBorderWidth = cs->ChangeBorderWidth;
    (*pScreen->ChangeBorderWidth) (pWin, bw);
    cs->ChangeBorderWidth = pScreen->ChangeBorderWidth;
    pScreen->ChangeBorderWidth = compChangeBorderWidth;

    compFreeOldPixmap(pWin);
    compCheckTree(pWin->drawable.pScreen);
}

void
compReparentWindow(WindowPtr pWin, WindowPtr pPriorParent)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);

    pScreen->ReparentWindow = cs->ReparentWindow;
    /*
     * Remove any implicit redirect due to synthesized visual
     */
    if (compImplicitRedirect(pWin, pPriorParent))
        compUnredirectWindow(serverClient, pWin, CompositeRedirectAutomatic);
    /*
     * Handle subwindows redirection
     */
    compUnredirectOneSubwindow(pPriorParent, pWin);
    compRedirectOneSubwindow(pWin->parent, pWin);
    /*
     * Add any implict redirect due to synthesized visual
     */
    if (compImplicitRedirect(pWin, pWin->parent))
        compRedirectWindow(serverClient, pWin, CompositeRedirectAutomatic);

    /*
     * Allocate any necessary redirect pixmap
     * (this actually should never be true; pWin is always unmapped)
     */
    compCheckRedirect(pWin);

    /*
     * Reset pixmap pointers as appropriate
     */
    if (pWin->parent && pWin->redirectDraw == RedirectDrawNone)
        compSetPixmap(pWin, (*pScreen->GetWindowPixmap) (pWin->parent));
    /*
     * Call down to next function
     */
    if (pScreen->ReparentWindow)
        (*pScreen->ReparentWindow) (pWin, pPriorParent);
    cs->ReparentWindow = pScreen->ReparentWindow;
    pScreen->ReparentWindow = compReparentWindow;
    compCheckTree(pWin->drawable.pScreen);
}

void
compCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    int dx = 0, dy = 0;

    if (pWin->redirectDraw != RedirectDrawNone) {
        PixmapPtr pPixmap = (*pScreen->GetWindowPixmap) (pWin);
        CompWindowPtr cw = GetCompWindow(pWin);

        assert(cw->oldx != COMP_ORIGIN_INVALID);
        assert(cw->oldy != COMP_ORIGIN_INVALID);
        if (cw->pOldPixmap) {
            /*
             * Ok, the old bits are available in pOldPixmap and
             * need to be copied to pNewPixmap.
             */
            RegionRec rgnDst;
            GCPtr pGC;

            dx = ptOldOrg.x - pWin->drawable.x;
            dy = ptOldOrg.y - pWin->drawable.y;
            RegionTranslate(prgnSrc, -dx, -dy);

            RegionNull(&rgnDst);

            RegionIntersect(&rgnDst, &pWin->borderClip, prgnSrc);

            RegionTranslate(&rgnDst, -pPixmap->screen_x, -pPixmap->screen_y);

            dx = dx + pPixmap->screen_x - cw->oldx;
            dy = dy + pPixmap->screen_y - cw->oldy;
            pGC = GetScratchGC(pPixmap->drawable.depth, pScreen);
            if (pGC) {
                BoxPtr pBox = RegionRects(&rgnDst);
                int nBox = RegionNumRects(&rgnDst);

                ValidateGC(&pPixmap->drawable, pGC);
                while (nBox--) {
                    (void) (*pGC->ops->CopyArea) (&cw->pOldPixmap->drawable,
                                                  &pPixmap->drawable,
                                                  pGC,
                                                  pBox->x1 + dx, pBox->y1 + dy,
                                                  pBox->x2 - pBox->x1,
                                                  pBox->y2 - pBox->y1,
                                                  pBox->x1, pBox->y1);
                    pBox++;
                }
                FreeScratchGC(pGC);
            }
            RegionUninit(&rgnDst);
            return;
        }
        dx = pPixmap->screen_x - cw->oldx;
        dy = pPixmap->screen_y - cw->oldy;
        ptOldOrg.x += dx;
        ptOldOrg.y += dy;
    }

    pScreen->CopyWindow = cs->CopyWindow;
    if (ptOldOrg.x != pWin->drawable.x || ptOldOrg.y != pWin->drawable.y) {
        if (dx || dy)
            RegionTranslate(prgnSrc, dx, dy);
        (*pScreen->CopyWindow) (pWin, ptOldOrg, prgnSrc);
        if (dx || dy)
            RegionTranslate(prgnSrc, -dx, -dy);
    }
    else {
        ptOldOrg.x -= dx;
        ptOldOrg.y -= dy;
        RegionTranslate(prgnSrc,
                        pWin->drawable.x - ptOldOrg.x,
                        pWin->drawable.y - ptOldOrg.y);
        DamageDamageRegion(&pWin->drawable, prgnSrc);
    }
    cs->CopyWindow = pScreen->CopyWindow;
    pScreen->CopyWindow = compCopyWindow;
    compCheckTree(pWin->drawable.pScreen);
}

Bool
compCreateWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    Bool ret;

    pScreen->CreateWindow = cs->CreateWindow;
    ret = (*pScreen->CreateWindow) (pWin);
    if (pWin->parent && ret) {
        CompSubwindowsPtr csw = GetCompSubwindows(pWin->parent);
        CompClientWindowPtr ccw;

        (*pScreen->SetWindowPixmap) (pWin,
                                     (*pScreen->GetWindowPixmap) (pWin->
                                                                  parent));
        if (csw)
            for (ccw = csw->clients; ccw; ccw = ccw->next)
                compRedirectWindow(clients[CLIENT_ID(ccw->id)],
                                   pWin, ccw->update);
        if (compImplicitRedirect(pWin, pWin->parent))
            compRedirectWindow(serverClient, pWin, CompositeRedirectAutomatic);
    }
    cs->CreateWindow = pScreen->CreateWindow;
    pScreen->CreateWindow = compCreateWindow;
    compCheckTree(pWin->drawable.pScreen);
    return ret;
}

Bool
compDestroyWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    CompWindowPtr cw;
    CompSubwindowsPtr csw;
    Bool ret;

    pScreen->DestroyWindow = cs->DestroyWindow;
    while ((cw = GetCompWindow(pWin)))
        FreeResource(cw->clients->id, RT_NONE);
    while ((csw = GetCompSubwindows(pWin)))
        FreeResource(csw->clients->id, RT_NONE);

    if (pWin->redirectDraw != RedirectDrawNone) {
        PixmapPtr pPixmap = (*pScreen->GetWindowPixmap) (pWin);

        compSetParentPixmap(pWin);
#ifdef ENABLE_DLOG
        SLOG(LOG_INFO, "XORG_PIXMAP", "pix_id:%p, pixmap:%p (%dx%d) refcnt:%d-1 win_id:%p win:%p (%dx%d) view:%d\n",
            pPixmap->drawable.id,
            pPixmap,
            pPixmap->drawable.width,
            pPixmap->drawable.height,
            pPixmap->refcnt,
            pWin->drawable.id,
            pWin,
            pWin->drawable.width,
            pWin->drawable.height,
            pWin->viewable);
#endif
        (*pScreen->DestroyPixmap) (pPixmap);
    }
    ret = (*pScreen->DestroyWindow) (pWin);
    cs->DestroyWindow = pScreen->DestroyWindow;
    pScreen->DestroyWindow = compDestroyWindow;
/*    compCheckTree (pWin->drawable.pScreen); can't check -- tree isn't good*/
    return ret;
}

void
compSetRedirectBorderClip(WindowPtr pWin, RegionPtr pRegion)
{
    CompWindowPtr cw = GetCompWindow(pWin);
    RegionRec damage;

    RegionNull(&damage);
    /*
     * Align old border clip with new border clip
     */
    RegionTranslate(&cw->borderClip,
                    pWin->drawable.x - cw->borderClipX,
                    pWin->drawable.y - cw->borderClipY);
    /*
     * Compute newly visible portion of window for repaint
     */
    RegionSubtract(&damage, pRegion, &cw->borderClip);
    /*
     * Report that as damaged so it will be redrawn
     */
    DamageDamageRegion(&pWin->drawable, &damage);
    RegionUninit(&damage);
    /*
     * Save the new border clip region
     */
    RegionCopy(&cw->borderClip, pRegion);
    cw->borderClipX = pWin->drawable.x;
    cw->borderClipY = pWin->drawable.y;
}

RegionPtr
compGetRedirectBorderClip(WindowPtr pWin)
{
    CompWindowPtr cw = GetCompWindow(pWin);

    return &cw->borderClip;
}

static void
compWindowUpdateAutomatic(WindowPtr pWin)
{
    CompWindowPtr cw = GetCompWindow(pWin);
    ScreenPtr pScreen = pWin->drawable.pScreen;
    WindowPtr pParent = pWin->parent;
    PixmapPtr pSrcPixmap = (*pScreen->GetWindowPixmap) (pWin);
    PictFormatPtr pSrcFormat = PictureWindowFormat(pWin);
    PictFormatPtr pDstFormat = PictureWindowFormat(pWin->parent);
    PicturePtr pSrcPicture = NULL;
    PicturePtr pDstPicture = NULL;
    int error;
    RegionPtr pRegion = DamageRegion(cw->damage);
    XID subwindowMode = IncludeInferiors;

    if (pSrcFormat)
        pSrcPicture = CreatePicture(0, &pSrcPixmap->drawable,
                                   pSrcFormat,
                                   0, 0,
                                   serverClient,
                                   &error);

    if (pDstFormat)
        pDstPicture = CreatePicture(0, &pParent->drawable,
                                   pDstFormat,
                                   CPSubwindowMode,
                                   &subwindowMode,
                                   serverClient,
                                   &error);

    /*
     * First move the region from window to screen coordinates
     */
    RegionTranslate(pRegion, pWin->drawable.x, pWin->drawable.y);

    /*
     * Clip against the "real" border clip
     */
    RegionIntersect(pRegion, pRegion, &cw->borderClip);

    /*
     * Now translate from screen to dest coordinates
     */
    RegionTranslate(pRegion, -pParent->drawable.x, -pParent->drawable.y);

    /*
     * Clip the picture
     */
    if (pDstPicture)
        SetPictureClipRegion(pDstPicture, 0, 0, pRegion);

    /*
     * And paint
     */
    if (pSrcPicture && pDstPicture)
        CompositePicture(PictOpSrc, pSrcPicture, 0, pDstPicture,
                         0, 0,      /* src_x, src_y */
                         0, 0,      /* msk_x, msk_y */
                         pSrcPixmap->screen_x - pParent->drawable.x,
                         pSrcPixmap->screen_y - pParent->drawable.y,
                         pSrcPixmap->drawable.width, pSrcPixmap->drawable.height);
    if (pSrcPicture)
        FreePicture(pSrcPicture, 0);
    if (pDstPicture)
        FreePicture(pDstPicture, 0);
    /*
     * Empty the damage region.  This has the nice effect of
     * rendering the translations above harmless
     */
    DamageEmpty(cw->damage);
}

static void
compPaintWindowToParent(WindowPtr pWin)
{
    compPaintChildrenToWindow(pWin);

    if (pWin->redirectDraw != RedirectDrawNone) {
        CompWindowPtr cw = GetCompWindow(pWin);

        if (cw->damaged) {
            compWindowUpdateAutomatic(pWin);
            cw->damaged = FALSE;
        }
    }
}

void
compPaintChildrenToWindow(WindowPtr pWin)
{
    WindowPtr pChild;

    if (!pWin->damagedDescendants)
        return;

    for (pChild = pWin->lastChild; pChild; pChild = pChild->prevSib)
        compPaintWindowToParent(pChild);

    pWin->damagedDescendants = FALSE;
}

WindowPtr
CompositeRealChildHead(WindowPtr pWin)
{
    WindowPtr pChild, pChildBefore;
    CompScreenPtr cs;

    if (!pWin->parent &&
        (screenIsSaved == SCREEN_SAVER_ON) &&
        (HasSaverWindow(pWin->drawable.pScreen))) {

        /* First child is the screen saver; see if next child is the overlay */
        pChildBefore = pWin->firstChild;
        pChild = pChildBefore->nextSib;

    }
    else {
        pChildBefore = NullWindow;
        pChild = pWin->firstChild;
    }

    if (!pChild) {
        return NullWindow;
    }

    cs = GetCompScreen(pWin->drawable.pScreen);
    if (pChild == cs->pOverlayWin) {
        return pChild;
    }
    else {
        return pChildBefore;
    }
}

int
compConfigNotify(WindowPtr pWin, int x, int y, int w, int h,
                 int bw, WindowPtr pSib)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    CompScreenPtr cs = GetCompScreen(pScreen);
    Bool ret = 0;
    WindowPtr pParent = pWin->parent;
    int draw_x, draw_y;
    Bool alloc_ret;

    if (cs->ConfigNotify) {
        pScreen->ConfigNotify = cs->ConfigNotify;
        ret = (*pScreen->ConfigNotify) (pWin, x, y, w, h, bw, pSib);
        cs->ConfigNotify = pScreen->ConfigNotify;
        pScreen->ConfigNotify = compConfigNotify;

        if (ret)
            return ret;
    }

    if (pWin->redirectDraw == RedirectDrawNone)
        return Success;

    compCheckTree(pScreen);

    draw_x = pParent->drawable.x + x + bw;
    draw_y = pParent->drawable.y + y + bw;
    alloc_ret = compReallocPixmap(pWin, draw_x, draw_y, w, h, bw);

    if (alloc_ret == FALSE)
        return BadAlloc;
    return Success;
}

#ifdef _F_INPUT_REDIRECTION_
static void
print_matrix(char* name, pixman_transform_t* t)
{
#ifdef _PRINT_INPUT_REDIRECT_MATRIX_
        int i;
        ErrorF("[%s] %s = \n", __FUNCTION__, name);
        for(i=0; i<3; i++)
        {
                ErrorF("\t%5.2f  %5.2f  %5.2f\n"
                        , pixman_fixed_to_double(t->matrix[i][0])
                        , pixman_fixed_to_double(t->matrix[i][1])
                        , pixman_fixed_to_double(t->matrix[i][2]));
        }
#endif //_PRINT_INPUT_REDIRECT_MATRIX_
}

typedef struct {
    CompositeSetTransformPtr SetTransform;
} CompositeScreenRec, *CompositeScreenPtr;

static DevPrivateKeyRec compositePrivateKeyRec;
#define compositePrivatePrivateKey (&compositePrivateKeyRec)

static CompositeScreenPtr
CompositeGetScreen(ScreenPtr pScreen)
{
    if (!dixPrivateKeyRegistered(compositePrivatePrivateKey))
        return NULL;
    return dixLookupPrivate(&pScreen->devPrivates, compositePrivatePrivateKey);
}

Bool
CompositeScreenInit (ScreenPtr pScreen, CompositeInfoPtr info)
{
    CompositeScreenPtr cs = NULL;

    if (!pScreen || !info)
        return FALSE;

    if (!dixRegisterPrivateKey(&compositePrivateKeyRec, PRIVATE_SCREEN, 0))
        return FALSE;

    cs = calloc(1, sizeof *cs);
    if (!cs)
        return FALSE;

    cs->SetTransform = info->SetTransform;

    dixSetPrivate(&pScreen->devPrivates, compositePrivatePrivateKey, cs);

    ErrorF ("[%s] setup complete\n", __FUNCTION__);

    return TRUE;
}

void
CompositeCloseScreen (ScreenPtr pScreen)
{
    CompositeScreenPtr cs = CompositeGetScreen(pScreen);

    if (cs)
        free (cs);
}

Bool
CompositeGetTransformPoint (WindowPtr pChild,
                         int      x,
                         int      y,
                         int      *tx,
                         int      *ty)
{
    CompWindowPtr cw = GetCompWindow (pChild);

    if (cw && cw->pTransform)
    {
        PictVector v;
        v.vector[0] = IntToxFixed(x);
        v.vector[1] = IntToxFixed(y);
        v.vector[2] = xFixed1;

        if(pixman_transform_point(cw->pTransform, &v))
        {
            *tx = xFixedToInt(v.vector[0]);
            *ty = xFixedToInt(v.vector[1]);
            return TRUE;
        }
        else
        {
            ErrorF("[%s] Failed on pixman_transform_point()\n", __FUNCTION__);
        }
    }

    *tx = x;
    *ty = y;

    /* There is no mesh, handle events like normal */
    if (!cw || !cw->pTransform)
       return TRUE;

    return FALSE;
}

Bool
CompositeGetInvTransformPoint (WindowPtr pChild,
                         int      x,
                         int      y,
                         int      *tx,
                         int      *ty)
{
    if(!pChild)
        return FALSE;
    CompWindowPtr cw = GetCompWindow (pChild);

    if (cw && cw->pInvTransform)
    {
        PictVector v;
        v.vector[0] = IntToxFixed(x);
        v.vector[1] = IntToxFixed(y);
        v.vector[2] = xFixed1;

        if(pixman_transform_point(cw->pInvTransform, &v))
        {
            *tx = xFixedToInt(v.vector[0]);
            *ty = xFixedToInt(v.vector[1]);
            return TRUE;
        }
        else
        {
            ErrorF("[%s] Failed on pixman_transform_point(cw->pInvTransform, &v)\n",__FUNCTION__);
        }
    }

    *tx = x;
    *ty = y;

    /* There is no mesh, handle events like normal */
    if (!cw || !cw->pInvTransform)
    {
       return TRUE;
    }

    return FALSE;
}

void
CompositeXYScreenToWindowRootCoordinate (WindowPtr pWin,
                                        int       x,
                                        int       y,
                                        int       *rootX,
                                        int       *rootY)
{
    if (!pWin->parent)
    {
       *rootX = x;
       *rootY = y;
    }
    else
    {
       CompositeXYScreenToWindowRootCoordinate (pWin->parent, x, y, &x, &y);
       CompositeGetInvTransformPoint(pWin, x, y, rootX, rootY);
    }
}

void
CompositeXYScreenFromWindowRootCoordinate (WindowPtr pWin,
                                          int       x,
                                          int       y,
                                          int       *screenX,
                                          int       *screenY)
{
#ifdef _F_STEREOSCOPIC_LEFT_BUFFER_COODINATE_
    static Bool preferred_resolution_set = IS_NOT_SET;
    if(!preferred_resolution_set)
    {
        ScreenPtr pScreen = pWin->drawable.pScreen;
        WindowPtr rootWin = pScreen->root;
        prefered_w = rootWin->drawable.width ;
        prefered_h = rootWin->drawable.height ;
        preferred_resolution_set = IS_SET;
    }
#endif

    if (!pWin->parent)
    {
       *screenX = x;
       *screenY = y;
    }
    else
    {
       CompositeGetTransformPoint(pWin, x, y, &x, &y);
       CompositeXYScreenFromWindowRootCoordinate (pWin->parent,
                                                  x, y, screenX, screenY);
    }
}

#ifdef _F_STEREOSCOPIC_LEFT_BUFFER_COODINATE_
void
CompositeXYScreenFromWindowLeftBufferCoordinate (WindowPtr pWin,
                                          int       x,
                                          int       y,
                                          int       *screenX,
                                          int       *screenY)
{

    CompositeXYScreenFromWindowRootCoordinate (pWin,x, y, screenX, screenY);

    int fbW, fbH ;
    ScreenPtr pScreen = pWin->drawable.pScreen;
    WindowPtr rootWin = pScreen->root;
    fbW = rootWin->drawable.width ;
    fbH = rootWin->drawable.height ;

    *screenX = (*screenX * prefered_w)/fbW ;
    *screenY = (*screenY * prefered_h)/fbH ;


}
#endif

int
CompositeSetCoordinateTransform (ClientPtr pClient,
                                     WindowPtr pWin,
                                     PictTransformPtr pTransform)
{
    CompSubwindowsPtr   csw = NULL;
    CompWindowPtr      cw = NULL;
    CompClientWindowPtr        ccw;
    PictTransform inv;
    CompositeScreenPtr cs = CompositeGetScreen(pWin->drawable.pScreen);

    if (!dixPrivateKeyRegistered(CompSubwindowsPrivateKey))
    {
        ErrorF("[%s:%d] Failed to CompSubwindowsPrivateKey\n", __FUNCTION__, __LINE__);
        return BadAccess;
    }

    csw = GetCompSubwindows (pWin->parent);

    if (!dixPrivateKeyRegistered(CompWindowPrivateKey))
    {
        ErrorF("[%s:%d] Failed to CompWindowPrivateKey\n", __FUNCTION__, __LINE__);
        return BadAccess;
    }

    cw = GetCompWindow (pWin);

    /*
     * sub-window must be Manual update
     */
    if (!csw || csw->update != CompositeRedirectManual)
    {
        ErrorF("[%s] BadAccess : !csw || csw->update != CompositeRedirectManual\n", __FUNCTION__);
        return BadAccess;
    }

    /*
     * must be Manual update client
     */
    for (ccw = csw->clients; ccw; ccw = ccw->next)
       if (ccw->update == CompositeRedirectManual &&
           CLIENT_ID (ccw->id) != pClient->index)
       {
           ErrorF("[%s] BadAccess : CLIENT_ID (ccw->id) != pClient->index\n", __FUNCTION__);
           return BadAccess;
       }

    if (pTransform && pixman_transform_is_identity(pTransform))
    {
#ifdef _PRINT_INPUT_REDIRECT_MATRIX_
        ErrorF("[%s] pTransform=0 : pTransform && pixman_transform_is_identity(pTransform)\n", __FUNCTION__);
#endif //_PRINT_INPUT_REDIRECT_MATRIX_
        pTransform = 0;
    }

    if (pTransform)
        print_matrix("pTransform", pTransform);
#ifdef _PRINT_INPUT_REDIRECT_MATRIX_
    else
        ErrorF("[%s] pTransform or pTransform->matrix is NULL !\n", __FUNCTION__);
#endif //_PRINT_INPUT_REDIRECT_MATRIX_

    if (pTransform && !pixman_transform_invert(&inv, pTransform))
    {
        ErrorF("[%s] BadRequest : !pixman_transform_invert(&inv, pTransform)\n", __FUNCTION__);
        return BadRequest;
    }

    if (pTransform)
    {
       if (!cw->pTransform)
       {
           cw->pTransform = (PictTransform *) xalloc (sizeof (PictTransform));
           if (!cw->pTransform)
           {
               ErrorF("[%s] BadAlloc : !cw->pTransform\n", __FUNCTION__);
               return BadAlloc;
           }
       }

       if (!cw->pInvTransform)
       {
           cw->pInvTransform = (PictTransform *) xalloc (sizeof (PictTransform));
           if (!cw->pInvTransform)
           {
               ErrorF("[%s] BadAlloc : !cw->pInvTransform\n", __FUNCTION__);
               return BadAlloc;
           }
       }

       *cw->pTransform = *pTransform;
       *cw->pInvTransform = inv;

       print_matrix("CW:transform", cw->pTransform);
       print_matrix("CW:inv transform", cw->pInvTransform);
    }
    else
    {
       if (cw->pTransform)
       {
           xfree (cw->pTransform);
           cw->pTransform = 0;
       }

       if (cw->pInvTransform)
       {
           xfree(cw->pInvTransform);
           cw->pInvTransform = 0;
       }
    }

    if (cs && cs->SetTransform)
    {
        int ret = cs->SetTransform(pWin, pTransform);

        if (ret != Success)
        {
            ErrorF("[%s] SetTransform return %d \n", __FUNCTION__, ret);
            return BadImplementation;
        }
    }

    CheckCursorConfinement(pWin);

    return 0;
}
#endif //_F_INPUT_REDIRECTION_
