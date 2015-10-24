#ifndef HWA_H_
#define HWA_H_

#include <xorg-server.h>

#include <X11/extensions/hwaproto.h>
#include <randrstr.h>

#define HWA_SCREEN_INFO_VERSION        1

typedef enum
{
    HWA_OVERLAY_LAYER_UI = 0,
    HWA_OVERLAY_LAYER_XV = 1
}HWA_OVERLAY_LAYER;

typedef int (*hwa_cursor_enable) (ScreenPtr screen, CARD16 *enable);
typedef int (*hwa_cursor_rotate) (ScreenPtr screen, RRCrtc crtc_id, CARD16 *pdegree);
typedef int (*hwa_screen_invert_negative_proc)(ScreenPtr pScreen, int accessibility_status);
typedef int (*hwa_screen_scale_proc)(ScreenPtr pScreen, int scale_status, int x, int y, int w, int h);
typedef int (*hwa_overlay_show_layer) (ScreenPtr screen, RRCrtc crtc_id, HWA_OVERLAY_LAYER ovrlayer);
typedef int (*hwa_overlay_hide_layer) (ScreenPtr screen, RRCrtc crtc_id, HWA_OVERLAY_LAYER ovrlayer);

typedef struct hwa_screen_info {
    uint32_t                    version;

    hwa_cursor_enable         cursor_enable;
    hwa_cursor_rotate         cursor_rotate;
    hwa_screen_invert_negative_proc  screen_invert_negative;
    hwa_screen_scale_proc            screen_scale;
    hwa_overlay_show_layer           show_layer;
    hwa_overlay_hide_layer           hide_layer;

} hwa_screen_info_rec, *hwa_screen_info_ptr;

extern _X_EXPORT Bool
hwa_screen_init(ScreenPtr screen, hwa_screen_info_ptr info);

#endif /* HWA_H_ */
