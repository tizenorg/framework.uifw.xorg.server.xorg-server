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

#ifndef _PRESENT_H_
#define _PRESENT_H_

#ifdef ENABLE_DLOG
#include <dlog.h>
#include <stdarg.h>
#define PRESENT_LOG_TAG "XORG_PIXMAP"
#define DebugPixmap(f, x...) {SLOG(LOG_INFO, PRESENT_LOG_TAG, f, ##x);}
#else
#define DebugPixmap(...) {;}
#endif

#include <X11/extensions/presentproto.h>
#include "randrstr.h"
#include "presentext.h"

typedef struct present_vblank present_vblank_rec, *present_vblank_ptr;

/* Return the current CRTC for 'window'.
 */
typedef RRCrtcPtr (*present_get_crtc_ptr) (WindowPtr window);

/* Return the current ust/msc for 'crtc'
 */
typedef int (*present_get_ust_msc_ptr) (RRCrtcPtr crtc, uint64_t *ust, uint64_t *msc);

/* Queue callback on 'crtc' for time 'msc'. Call present_event_notify with 'event_id'
 * at or after 'msc'. Return false if it didn't happen (which might occur if 'crtc'
 * is not currently generating vblanks).
 */
typedef Bool (*present_queue_vblank_ptr) (RRCrtcPtr crtc,
                                          uint64_t event_id,
                                          uint64_t msc);

/* Abort pending vblank. The extension is no longer interested in
 * 'event_id' which was to be notified at 'msc'. If possible, the
 * driver is free to de-queue the notification.
 */
typedef void (*present_abort_vblank_ptr) (RRCrtcPtr crtc, uint64_t event_id, uint64_t msc);

/* Flush pending drawing on 'window' to the hardware.
 */
typedef void (*present_flush_ptr) (WindowPtr window);

/* Check if 'pixmap' is suitable for flipping to 'window'.
 */
typedef Bool (*present_check_flip_ptr) (RRCrtcPtr crtc, WindowPtr window, PixmapPtr pixmap, Bool sync_flip);

/* Flip pixmap, return false if it didn't happen.
 *
 * 'crtc' is to be used for any necessary synchronization.
 *
 * 'sync_flip' requests that the flip be performed at the next
 * vertical blank interval to avoid tearing artifacts. If false, the
 * flip should be performed as soon as possible.
 *
 * present_event_notify should be called with 'event_id' when the flip
 * occurs
 */
typedef Bool (*present_flip_ptr) (RRCrtcPtr crtc,
                                  uint64_t event_id,
                                  uint64_t target_msc,
                                  PixmapPtr pixmap,
                                  Bool sync_flip);
/* #ifdef _F_PRESENT_HWC_FLIP_ */
/* Add in for Tizen.
 * Flip pixmap, return false if it didn't happen.
 *
 * 'crtc' is to be used for any necessary synchronization.
 *
 * 'sync_flip' requests that the flip be performed at the next
 * vertical blank interval to avoid tearing artifacts. If false, the
 * flip should be performed as soon as possible.
 *
 * present_event_notify should be called with 'event_id' when the flip
 * occurs
 */
typedef Bool (*present_window_flip_ptr) (RRCrtcPtr crtc,
                                  uint64_t event_id,
                                  uint64_t target_msc,
                                  WindowPtr window,
                                  PixmapPtr pixmap,
                                  Bool sync_flip);
/* #endif */

/* "unflip" back to the regular screen scanout buffer
 *
 * present_event_notify should be called with 'event_id' when the unflip occurs.
 */
typedef void (*present_unflip_ptr) (ScreenPtr screen,
                                    uint64_t event_id);

typedef void (*present_get_scanout_flag_ptr) (WindowPtr window,
                                    uint32_t *flag);

#define PRESENT_SCREEN_INFO_VERSION        0

typedef struct present_screen_info {
    uint32_t                            version;

    present_get_crtc_ptr                get_crtc;
    present_get_ust_msc_ptr             get_ust_msc;
    present_queue_vblank_ptr            queue_vblank;
    present_abort_vblank_ptr            abort_vblank;
    present_flush_ptr                   flush;
    uint32_t                            capabilities;
    present_check_flip_ptr              check_flip;
    present_flip_ptr                    flip;
    present_unflip_ptr                  unflip;
/* #ifdef _F_PRESENT_HWC_FLIP_ */
    present_window_flip_ptr             window_flip;
    present_get_scanout_flag_ptr        get_scanout_flag;
/* #endif */

} present_screen_info_rec, *present_screen_info_ptr;

/*
 * Called when 'event_id' occurs. 'ust' and 'msc' indicate when the
 * event actually happened
 */
extern _X_EXPORT void
present_event_notify(uint64_t event_id, uint64_t ust, uint64_t msc);

/* 'crtc' has been turned off, so any pending events will never occur.
 */
extern _X_EXPORT void
present_event_abandon(RRCrtcPtr crtc);

extern _X_EXPORT Bool
present_screen_init(ScreenPtr screen, present_screen_info_ptr info);

typedef void (*present_complete_notify_proc)(WindowPtr window,
                                             CARD8 mode,
                                             CARD32 serial,
                                             uint64_t ust,
                                             uint64_t msc);

extern _X_EXPORT void
present_register_complete_notify(present_complete_notify_proc proc);

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
extern _X_EXPORT void
present_send_scanout_notify(WindowPtr window, CARD16 scanout);

extern _X_EXPORT void
present_send_tree_scanout_notify(WindowPtr window, CARD16 scanout);
#endif //_F_PRESENT_SCANOUT_NOTIFY_

#endif /* _PRESENT_H_ */
