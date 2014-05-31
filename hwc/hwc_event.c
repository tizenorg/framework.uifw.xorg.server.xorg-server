/*
 * Copyright © 2013 samsung
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

#include "hwc_priv.h"

RESTYPE hwc_event_type;

void
hwc_free_events(WindowPtr window)
{
    hwc_screen_priv_ptr screen_priv = hwc_screen_priv(window->drawable.pScreen);
    hwc_event_ptr *previous, current=NULL;
    
    if (!screen_priv)
        return;

    for (previous = &screen_priv->events; (current = *previous); previous = &current->next) {
        if (current->window == window) {
            *previous = current->next;
            break;
        }
    }

    if(current)
        free((pointer) current);
}

static void
hwc_event_swap(xGenericEvent *from, xGenericEvent *to)
{
    *to = *from;
    swaps(&to->sequenceNumber);
    swapl(&to->length);
    swaps(&to->evtype);
    switch (from->evtype) {
    case HWCConfigureNotify: {
        xHWCConfigureNotify *c = (xHWCConfigureNotify *) to;

        swapl(&c->maxLayer);
        break;
    }
    }
}

void
hwc_send_config_notify(ScreenPtr screen, int maxLayer)
{
    hwc_screen_priv_ptr screen_priv = hwc_screen_priv(screen);

    if (screen_priv) {
        xHWCConfigureNotify cn = {
            .type = GenericEvent,
            .extension = hwc_request,
            .length = (sizeof(xHWCConfigureNotify) - 32) >> 2,
            .evtype = HWCConfigureNotify,
            .maxLayer = maxLayer,
        };
        hwc_event_ptr event;

        for (event = screen_priv->events; event; event = event->next) {
            if (event->mask & (1 << HWCConfigureNotify)) {
                WriteEventsToClient(event->client, 1, (xEvent *) &cn);
            }
        }
    }
}

int
hwc_select_input(ClientPtr client, ScreenPtr screen, WindowPtr window, CARD32 mask)
{
    hwc_screen_priv_ptr screen_priv = hwc_screen_priv(screen);
    hwc_event_ptr event;

    if (!screen_priv)
        return BadAlloc;

    event = calloc (1, sizeof (hwc_event_rec));
    if (!event)
        return BadAlloc;

    event->client = client;
    event->screen = screen;
    event->window = window;
    event->mask = mask;

    event->next = screen_priv->events;
    screen_priv->events = event;

    return Success;
}

Bool
hwc_event_init(void)
{
    GERegisterExtension(hwc_request, hwc_event_swap);
    return TRUE;
}
