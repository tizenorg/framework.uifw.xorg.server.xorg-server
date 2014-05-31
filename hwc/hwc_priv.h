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

#ifndef _HWCPRIV_H_
#define _HWCPRIV_H_

#include <X11/X.h>
#include "scrnintstr.h"
#include "misc.h"
#include "list.h"
#include "windowstr.h"
#include "dixstruct.h"
#include "hwc.h"

extern int hwc_request;

/*
 * Each screen has a list of clients and event masks
 */
typedef struct hwc_event *hwc_event_ptr;

typedef struct hwc_event {
    hwc_event_ptr next;
    ClientPtr client;
    ScreenPtr screen;
    WindowPtr window;
    int mask;
} hwc_event_rec;

extern DevPrivateKeyRec hwc_screen_private_key;

typedef struct hwc_screen_priv {
    CloseScreenProcPtr          CloseScreen;
    ConfigNotifyProcPtr         ConfigNotify;
    DestroyWindowProcPtr        DestroyWindow;

    hwc_screen_info_ptr        info;
    hwc_event_ptr events;
} hwc_screen_priv_rec, *hwc_screen_priv_ptr;

#define wrap(priv,real,mem,func) {\
    priv->mem = real->mem; \
    real->mem = func; \
}

#define unwrap(priv,real,mem) {\
    real->mem = priv->mem; \
}

static inline hwc_screen_priv_ptr
hwc_screen_priv(ScreenPtr screen)
{
    return (hwc_screen_priv_ptr)dixLookupPrivate(&(screen)->devPrivates, &hwc_screen_private_key);
}

int
proc_hwc_dispatch(ClientPtr client);

int
sproc_hwc_dispatch(ClientPtr client);

Bool
hwc_event_init(void);

void
hwc_free_events(WindowPtr window);

int
hwc_select_input(ClientPtr client, ScreenPtr screen, WindowPtr window, CARD32 mask);
/* DDX interface */

#endif /* _HWCPRIV_H_ */
