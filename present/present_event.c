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

RESTYPE present_event_type;

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
#include "compint.h"

static WindowPtr
_FindTopLevelWindowPtr (ScreenPtr screen, WindowPtr pWin)
{
    WindowPtr pRoot = screen->root;
    while (pWin->parent && pWin->parent->drawable.id != pRoot->drawable.id)
        pWin = pWin->parent;

    return pWin;
}
#endif //_F_PRESENT_SCANOUT_NOTIFY_

static int
present_free_event(void *data, XID id)
{
    present_event_ptr present_event = (present_event_ptr) data;
    present_window_priv_ptr window_priv = present_window_priv(present_event->window);
    present_event_ptr *previous, current;

    for (previous = &window_priv->events; (current = *previous); previous = &current->next) {
        if (current == present_event) {
            *previous = present_event->next;
            break;
        }
    }
    free((void *) present_event);
    return 1;

}

void
present_free_events(WindowPtr window)
{
    present_window_priv_ptr window_priv = present_window_priv(window);
    present_event_ptr event;

    if (!window_priv)
        return;

    while ((event = window_priv->events))
        FreeResource(event->id, RT_NONE);
}

static void
present_event_swap(xGenericEvent *from, xGenericEvent *to)
{
    *to = *from;
    swaps(&to->sequenceNumber);
    swapl(&to->length);
    swaps(&to->evtype);
    switch (from->evtype) {
    case PresentConfigureNotify: {
        xPresentConfigureNotify *c = (xPresentConfigureNotify *) to;

        swapl(&c->eid);
        swapl(&c->window);
        swaps(&c->x);
        swaps(&c->y);
        swaps(&c->width);
        swaps(&c->height);
        swaps(&c->off_x);
        swaps(&c->off_y);
        swaps(&c->pixmap_width);
        swaps(&c->pixmap_height);
        swapl(&c->pixmap_flags);
        break;
    }
    case PresentCompleteNotify:
    {
        xPresentCompleteNotify *c = (xPresentCompleteNotify *) to;
        swapl(&c->eid);
        swapl(&c->window);
        swapl(&c->serial);
        swapll(&c->ust);
        swapll(&c->msc);
        break;
    }
    case PresentIdleNotify:
    {
        xPresentIdleNotify *c = (xPresentIdleNotify *) to;
        swapl(&c->eid);
        swapl(&c->window);
        swapl(&c->serial);
        swapl(&c->idle_fence);
        break;
    }
#ifdef _F_PRESENT_SCANOUT_NOTIFY_
    case PresentScanoutNotify:
    {
        xPresentScanoutNotify *c = (xPresentScanoutNotify *) to;
        swapl(&c->eid);
        swapl(&c->window);
        swaps(&c->scanout);
        break;
    }
#endif //_F_PRESENT_SCANOUT_NOTIFY_
    }
}

void
present_send_config_notify(WindowPtr window, int x, int y, int w, int h, int bw, WindowPtr sibling)
{
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (window_priv) {
        xPresentConfigureNotify cn = {
            .type = GenericEvent,
            .extension = present_request,
            .length = (sizeof(xPresentConfigureNotify) - 32) >> 2,
            .evtype = PresentConfigureNotify,
            .eid = 0,
            .window = window->drawable.id,
            .x = x,
            .y = y,
            .width = w,
            .height = h,
            .off_x = 0,
            .off_y = 0,
            .pixmap_width = w,
            .pixmap_height = h,
            .pixmap_flags = 0
        };
        present_event_ptr event;

        for (event = window_priv->events; event; event = event->next) {
            if (event->mask & (1 << PresentConfigureNotify)) {
                cn.eid = event->id;
                WriteEventsToClient(event->client, 1, (xEvent *) &cn);
            }
        }
    }
}

static present_complete_notify_proc complete_notify;

void
present_register_complete_notify(present_complete_notify_proc proc)
{
    complete_notify = proc;
}

void
present_send_complete_notify(WindowPtr window, CARD8 kind, CARD8 mode, CARD32 serial, uint64_t ust, uint64_t msc)
{
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (window_priv) {
        xPresentCompleteNotify cn = {
            .type = GenericEvent,
            .extension = present_request,
            .length = (sizeof(xPresentCompleteNotify) - 32) >> 2,
            .evtype = PresentCompleteNotify,
            .kind = kind,
            .mode = mode,
            .eid = 0,
            .window = window->drawable.id,
            .serial = serial,
            .ust = ust,
            .msc = msc,
        };
        present_event_ptr event;

        for (event = window_priv->events; event; event = event->next) {
            if (event->mask & PresentCompleteNotifyMask) {
                cn.eid = event->id;
                WriteEventsToClient(event->client, 1, (xEvent *) &cn);
            }
        }
    }
    if (complete_notify)
        (*complete_notify)(window, mode, serial, ust, msc);
}

void
present_send_idle_notify(WindowPtr window, CARD32 serial, PixmapPtr pixmap, struct present_fence *idle_fence)
{
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (window_priv) {
        xPresentIdleNotify in = {
            .type = GenericEvent,
            .extension = present_request,
            .length = (sizeof(xPresentIdleNotify) - 32) >> 2,
            .evtype = PresentIdleNotify,
            .eid = 0,
            .window = window->drawable.id,
            .serial = serial,
            .pixmap = pixmap->drawable.id,
            .idle_fence = present_fence_id(idle_fence)
        };
        present_event_ptr event;

        for (event = window_priv->events; event; event = event->next) {
            if (event->mask & PresentIdleNotifyMask) {
                in.eid = event->id;
                WriteEventsToClient(event->client, 1, (xEvent *) &in);
            }
        }
    }
}

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
void
present_send_scanout_notify(WindowPtr window, CARD16 scanout)
{
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (window_priv) {
        present_screen_priv_ptr screen_priv = present_screen_priv(window->drawable.pScreen);
        uint32_t flag = 0;

        if (screen_priv->info->get_scanout_flag)
            (*screen_priv->info->get_scanout_flag) (window, &flag);

        xPresentScanoutNotify in = {
            .type = GenericEvent,
            .extension = present_request,
            .length = (sizeof(xPresentScanoutNotify) - 32) >> 2,
            .evtype = PresentScanoutNotify,
            .eid = 0,
            .window = window->drawable.id,
            .scanout = scanout,
            .flag = flag
        };
        present_event_ptr event;

        for (event = window_priv->events; event; event = event->next) {
            if (event->mask & PresentScanoutNotifyMask) {
                in.eid = event->id;
                WriteEventsToClient(event->client, 1, (xEvent *) &in);
            }
        }
    }
}

static int
present_send_scanout_notify_visit(WindowPtr window, void *data)
{
    CARD16 scanout = *(CARD16 *)data;

    present_send_scanout_notify (window, scanout);
    return WT_WALKCHILDREN;
}

void
present_send_tree_scanout_notify (WindowPtr window, CARD16 scanout)
{
    TraverseTree(window, present_send_scanout_notify_visit, (void *)&scanout);
}
#endif //_F_PRESENT_SCANOUT_NOTIFY_

int
present_select_input(ClientPtr client, XID eid, WindowPtr window, CARD32 mask)
{
    present_window_priv_ptr window_priv = present_get_window_priv(window, mask != 0);
    present_event_ptr event;

    if (!window_priv) {
        if (mask)
            return BadAlloc;
        return Success;
    }

    event = calloc (1, sizeof (present_event_rec));
    if (!event)
        return BadAlloc;

    event->client = client;
    event->window = window;
    event->id = eid;
    event->mask = mask;

    event->next = window_priv->events;
    window_priv->events = event;

    if (!AddResource(event->id, present_event_type, (void *) event))
        return BadAlloc;

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
    if (mask & PresentScanoutNotify) {
        CompScreenPtr pCompScreen = GetCompScreen (window->drawable.pScreen);
        WindowPtr toplevel_window = _FindTopLevelWindowPtr (window->drawable.pScreen, window);
        CARD16 scanout = 0;

        if (toplevel_window->drawable.id == pCompScreen->overlayWid)
            scanout = 1;

        present_send_scanout_notify (window, scanout);
    }
#endif //_F_PRESENT_SCANOUT_NOTIFY_

    return Success;
}

Bool
present_event_init(void)
{
    present_event_type = CreateNewResourceType(present_free_event, "PresentEvent");
    if (!present_event_type)
        return FALSE;

    GERegisterExtension(present_request, present_event_swap);
    return TRUE;
}
