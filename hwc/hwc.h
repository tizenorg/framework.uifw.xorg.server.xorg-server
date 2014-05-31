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

#ifndef _HWC_H_
#define _HWC_H_

#include <X11/extensions/hwcproto.h>

#define HWC_SCREEN_INFO_VERSION        0

typedef int (*hwc_open_proc)(ScreenPtr screen, int *maxLayer);

typedef int (*hwc_set_drawables_proc) (ScreenPtr screen,
                                               DrawablePtr *drawables,
                                               int count);

typedef int (*hwc_update_drawable) (ScreenPtr screen,
                                                DrawablePtr drawable,
                                                int x,
                                                int y,
                                                int w,
                                                int h);
                                                
typedef struct hwc_screen_info {
    uint32_t                    version;
    int                            maxLayer;        

    hwc_open_proc              open;
    hwc_set_drawables_proc    set_drawables;
    hwc_update_drawable     update_drawable;
} hwc_screen_info_rec, *hwc_screen_info_ptr;

extern _X_EXPORT Bool
hwc_screen_init(ScreenPtr screen, hwc_screen_info_ptr info);

extern _X_EXPORT void
hwc_send_config_notify(ScreenPtr screen, int maxLayer);

int
hwc_open(ClientPtr client, ScreenPtr screen, int *maxLayer);

int
hwc_set_drawables(ClientPtr client, ScreenPtr screen, XID* drawables, int count);

#endif /* _HWC_H_ */
