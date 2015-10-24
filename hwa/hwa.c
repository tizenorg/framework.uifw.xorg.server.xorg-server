#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "hwa_priv.h"

int hwa_request;

DevPrivateKeyRec hwa_screen_private_key;
DevPrivateKeyRec hwa_window_private_key;

static Bool
hwa_close_screen(ScreenPtr screen)
{
    hwa_screen_priv_ptr screen_priv = hwa_screen_priv(screen);

    unwrap(screen_priv, screen, CloseScreen);

    free(screen_priv);
    return (*screen->CloseScreen) (screen);

}

Bool
hwa_screen_init(ScreenPtr screen, hwa_screen_info_ptr info)
{
    if (!dixRegisterPrivateKey(&hwa_screen_private_key, PRIVATE_SCREEN, 0))
        return FALSE;

    if (!hwa_screen_priv(screen)) {
        hwa_screen_priv_ptr screen_priv = calloc(1, sizeof (hwa_screen_priv_rec));
        if (!screen_priv)
            return FALSE;

        wrap(screen_priv, screen, CloseScreen, hwa_close_screen);
        /*
        wrap(screen_priv, screen, ConfigNotify, hwa_config_notify);
        */

        screen_priv->info = info;

        dixSetPrivate(&screen->devPrivates, &hwa_screen_private_key, screen_priv);
    }

    return TRUE;
}

/*
 * Initialization of HWA extension
 */
void
hwa_extension_init(void)
{
    ExtensionEntry *extension;
    int i;

    extension = AddExtension( HWA_NAME, HWANumberRequests, HWANumberErrors,
                proc_hwa_dispatch, sproc_hwa_dispatch,
                             NULL, StandardMinorOpcode );
    if (!extension)
        goto bail;

    hwa_request = extension->base;

    for (i = 0; i < screenInfo.numScreens; i++) {
        if (!hwa_screen_init(screenInfo.screens[i], NULL))
            goto bail;
    }
    return;

bail:
    FatalError("Cannot initialize HWA extension");
}
