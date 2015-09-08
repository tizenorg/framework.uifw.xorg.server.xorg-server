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
    ScreenPtr screen;
    int maxLayer = 0;
    int status;

    REQUEST_SIZE_MATCH(xHWCOpenReq);

    status = dixLookupWindow(&window, stuff->window, client, DixReadAccess);
    if (status != Success)
        return status;

    screen = window->drawable.pScreen;

    status = hwc_open(client, screen, &maxLayer);
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
    int status;
    int i;
    xHWCDrawInfo *drawInfo;
    XID *drawables;
    xRectangle *srcRects;
    xRectangle *dstRects;

    REQUEST_FIXED_SIZE(xHWCSetDrawablesReq, stuff->count * 20);

    status  = dixLookupWindow(&window, stuff->window, client, DixReadAccess);
    if (status != Success)
        return status;

    screen = window->drawable.pScreen;

    drawables = (XID *)calloc(1, sizeof(XID) * stuff->count);
    srcRects = (xRectangle *)calloc(1, sizeof(xRectangle) * stuff->count);
    dstRects = (xRectangle *)calloc(1, sizeof(xRectangle) * stuff->count);
    //drawables = (XID*) &stuff[1];
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
        ErrorF("srcRects[%d] - drawable(%d) src_rect(%d,%d,%d,%d) dst_rect(%d,%d,%d,%d)  count(%d)\n",
                i,drawInfo[i].drawable,srcRects[i].x, srcRects[i].y, srcRects[i].width, srcRects[i].height,
                 dstRects[i].x, dstRects[i].y, dstRects[i].width, dstRects[i].height, stuff->count);
    }

    status = hwc_set_drawables(client, screen, drawables, srcRects, dstRects, stuff->count);

    free(drawables);
    free(srcRects);
    free(dstRects);

    return status;
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
    proc_hwc_set_drawables,       /* 2 */
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
    return (*proc_hwc_vector[stuff->hwcReqType]) (client);
}

static int
sproc_hwc_set_drawables(ClientPtr client)
{
    REQUEST(xHWCSetDrawablesReq);

    swaps(&stuff->length);
    swapl(&stuff->window);
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
