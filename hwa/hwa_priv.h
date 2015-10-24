#ifndef HWA_PRIV_H_
#define HWA_PRIV_H_

#define HAVE_STRNDUP
#include <X11/X.h>
#include "scrnintstr.h"
#include "misc.h"
#include "list.h"
#include "windowstr.h"
#include "dixstruct.h"
#include <randrstr.h>
#include "hwa.h"

extern int hwa_request;
extern DevPrivateKeyRec hwa_screen_private_key;

typedef struct hwa_screen_priv {
    CloseScreenProcPtr          CloseScreen;
    ConfigNotifyProcPtr         ConfigNotify;
    DestroyWindowProcPtr        DestroyWindow;

    hwa_screen_info_ptr         info;
} hwa_screen_priv_rec, *hwa_screen_priv_ptr;

#define wrap(priv,real,mem,func) {\
    priv->mem = real->mem; \
    real->mem = func; \
}

#define unwrap(priv,real,mem) {\
    real->mem = priv->mem; \
}

/*
 * ...
 */
static inline hwa_screen_priv_ptr
hwa_screen_priv(ScreenPtr screen)
{
    return (hwa_screen_priv_ptr)dixLookupPrivate(&(screen)->devPrivates, &hwa_screen_private_key);
}

/* Initialize main request handlers */
int
proc_hwa_dispatch(ClientPtr client);

/* Initialize auxiliary request handlers */
int
sproc_hwa_dispatch(ClientPtr client);

/* DDX interface */

int
hwa_CursorEnable( ScreenPtr screen, CARD16 *penable);

int
hwa_CursorRotate( ScreenPtr screen, RRCrtc crtc_id, CARD16 *pdegree);

int
hwa_screen_invert_negative( RRCrtcPtr target_crtc, int accessibility_status );

int
hwa_screen_scale(RRCrtcPtr target_crtc, int scale_status, int x, int y, int w, int h);

int
hwa_ShowLayer( ScreenPtr pScreen, RRCrtc crtc_id, HWA_OVERLAY_LAYER ovrlayer);

int
hwa_HideLayer( ScreenPtr pScreen, RRCrtc crtc_id, HWA_OVERLAY_LAYER ovrlayer);

#endif /* HWA_PRIV_H_ */




