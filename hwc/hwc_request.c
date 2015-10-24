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
#include <syncsrv.h>
#include <unistd.h>
#include <xace.h>
#include "randrstr.h"

#define VERIFY_CRTC_OR_NONE(crtc_ptr, crtc_id, client, access) do {     \
        if ((crtc_id) == None)                                          \
            (crtc_ptr) = NULL;                                          \
        else {                                                          \
            VERIFY_RR_CRTC(crtc_id, crtc_ptr, access);                  \
        }                                                               \
    } while (0)

static int
proc_hwc_query_version(ClientPtr client)
{
    REQUEST(xHWCQueryVersionReq);
    xHWCQueryVersionReply rep = {
        .type = X_Reply,
        .sequenceNumber = client->sequence,
        .length = 0,
        .majorVersion = HWC_MAJOR,
        .minorVersion = HWC_MINOR
    };

    REQUEST_SIZE_MATCH(xHWCQueryVersionReq);
    (void) stuff;
    if (client->swapped) {
        swaps(&rep.sequenceNumber);
        swapl(&rep.length);
        swapl(&rep.majorVersion);
        swapl(&rep.minorVersion);
    }

    WriteToClient(client, sizeof(rep), &rep);
    return Success;
}

static int
proc_hwc_open(ClientPtr client)
{
    REQUEST(xHWCOpenReq);
    xHWCOpenReply rep = {
        .type = X_Reply,
        .sequenceNumber = client->sequence,
        .length = 0,
        .maxLayer = 0
    };
    WindowPtr window;
    RRCrtcPtr crtc;
    ScreenPtr screen;
    int maxLayer = 0;
    int status;

    REQUEST_SIZE_MATCH(xHWCOpenReq);

    VERIFY_CRTC_OR_NONE(crtc, stuff->crtc, client, DixReadAccess);

    if (crtc != 0)
        screen = crtc->pScreen;
    else
        screen = screenInfo.screens[0];     //get default screen

    status = hwc_open(client, screen, crtc, &maxLayer);
    if (status != Success)
        return status;

    if (client->swapped) {
        swaps(&rep.sequenceNumber);
        swapl(&rep.length);
    }

    rep.maxLayer = maxLayer;
    WriteToClient(client, sizeof (rep), &rep);

    return Success;
}

static int
proc_hwc_set_drawables(ClientPtr client)
{
    REQUEST(xHWCSetDrawablesReq);
    WindowPtr window;
    ScreenPtr screen;
    RRCrtcPtr crtc;
    int status;
    int i;
    xHWCDrawInfo *drawInfo;
    XID *drawables;
    xRectangle *srcRects = NULL;
    xRectangle *dstRects = NULL;
    HWCCompositeMethod *compMeth = NULL;

    REQUEST_FIXED_SIZE(xHWCSetDrawablesReq, stuff->count * sz_xHWCDrawInfo);

    VERIFY_CRTC_OR_NONE(crtc, stuff->crtc, client, DixReadAccess);

    if (crtc != 0)
        screen = crtc->pScreen;
    else
        screen = screenInfo.screens[0];     //get default screen

    drawables = (XID *)calloc(1, sizeof(XID) * stuff->count);
    if (!drawables)
    {
        ErrorF("drawables calloc fail\n");
        goto fail;
    }
    srcRects = (xRectangle *)calloc(1, sizeof(xRectangle) * stuff->count);
    if (!srcRects)
    {
        ErrorF("srcRects calloc fail\n");
        goto fail;
    }
    dstRects = (xRectangle *)calloc(1, sizeof(xRectangle) * stuff->count);
    if (!dstRects)
    {
        ErrorF("dstRects calloc fail\n");
        goto fail;
    }
    compMeth = (xRectangle *)calloc(1, sizeof(HWCCompositeMethod) * stuff->count);
    if (!compMeth)
    {
        ErrorF("compMeth calloc fail\n");
        goto fail;
    }
    drawInfo = (xHWCDrawInfo *) &stuff[1];
    for(i=0; i<stuff->count ; i++)
    {
        drawables[i] = drawInfo[i].drawable;
        srcRects[i].x = drawInfo[i].srcX;
        srcRects[i].y = drawInfo[i].srcY;
        srcRects[i].width = drawInfo[i].srcWidth;
        srcRects[i].height = drawInfo[i].srcHeight;
        dstRects[i].x = drawInfo[i].dstX;
        dstRects[i].y = drawInfo[i].dstY;
        dstRects[i].width = drawInfo[i].dstWidth;
        dstRects[i].height = drawInfo[i].dstHeight;
        compMeth[i] = drawInfo[i].compMethod;
        ErrorF("crtc(%d) srcRects[%d] - drawable(%d) src_rect(%d,%d,%d,%d) dst_rect(%d,%d,%d,%d) copm(%d) count(%d)\n",
                stuff->crtc, i,drawInfo[i].drawable,srcRects[i].x, srcRects[i].y, srcRects[i].width, srcRects[i].height,
                 dstRects[i].x, dstRects[i].y, dstRects[i].width, dstRects[i].height, stuff->count);
    }

    status = hwc_set_drawables(client, screen, crtc, drawables, srcRects, dstRects, compMeth, stuff->count);

    free(drawables);
    free(srcRects);
    free(dstRects);
    free(compMeth);

    return status;

fail:
    if (drawables)
        free(drawables);
    if (srcRects)
        free(srcRects);
    if (dstRects)
        free(dstRects);

    return BadAlloc;
}

static int
proc_hwc_select_input (ClientPtr client)
{
    REQUEST(xHWCSelectInputReq);
    WindowPtr window;
    ScreenPtr screen;
    int rc;

    REQUEST_SIZE_MATCH(xHWCSelectInputReq);

    rc = dixLookupWindow(&window, stuff->window, client, DixGetAttrAccess);
    if (rc != Success)
        return rc;

    screen = window->drawable.pScreen;
    
    if (stuff->eventMask & ~HWCAllEvents) {
        client->errorValue = stuff->eventMask;
        return BadValue;
    }
    
    return hwc_select_input(client, screen, window, stuff->eventMask);
}

int (*proc_hwc_vector[HWCNumberRequests]) (ClientPtr) = {
    proc_hwc_query_version,            /* 0 */
    proc_hwc_open,                     /* 1 */
    proc_hwc_set_drawables,            /* 2 */
    proc_hwc_select_input,              /*3*/
};

int
proc_hwc_dispatch(ClientPtr client)
{
    REQUEST(xReq);
    if (stuff->data >= HWCNumberRequests || !proc_hwc_vector[stuff->data])
        return BadRequest;
    return (*proc_hwc_vector[stuff->data]) (client);
}

static int
sproc_hwc_query_version(ClientPtr client)
{
    REQUEST(xHWCQueryVersionReq);

    swaps(&stuff->length);
    swapl(&stuff->majorVersion);
    swapl(&stuff->minorVersion);
    return (*proc_hwc_vector[stuff->hwcReqType]) (client);
}

static int
sproc_hwc_open(ClientPtr client)
{
    REQUEST(xHWCOpenReq);

    swaps(&stuff->length);
    swapl(&stuff->window);
    swapl(&stuff->crtc);
    return (*proc_hwc_vector[stuff->hwcReqType]) (client);
}

static int
sproc_hwc_set_drawables(ClientPtr client)
{
    REQUEST(xHWCSetDrawablesReq);

    swaps(&stuff->length);
    swapl(&stuff->window);
    swapl(&stuff->crtc);
    swapl(&stuff->count);
    return (*proc_hwc_vector[stuff->hwcReqType]) (client);
}

static int
sproc_hwc_select_input(ClientPtr client)
{
    REQUEST(xHWCSelectInputReq);

    swaps(&stuff->length);
    swapl(&stuff->window);
    swapl(&stuff->eventMask);
    return (*proc_hwc_vector[stuff->hwcReqType]) (client);
}

int (*sproc_hwc_vector[HWCNumberRequests]) (ClientPtr) = {
    sproc_hwc_query_version,           /* 0 */
    sproc_hwc_open,                    /* 1 */
    sproc_hwc_set_drawables,      /* 2 */
    sproc_hwc_select_input,             /* 3 */
};

int
sproc_hwc_dispatch(ClientPtr client)
{
    REQUEST(xReq);
    if (stuff->data >= HWCNumberRequests || !sproc_hwc_vector[stuff->data])
        return BadRequest;
    return (*sproc_hwc_vector[stuff->data]) (client);
}
