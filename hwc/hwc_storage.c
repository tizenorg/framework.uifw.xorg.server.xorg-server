#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "hwc_priv.h"

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
#include "present.h"
#endif //_F_PRESENT_SCANOUT_NOTIFY_

/*
 *  Storage of drawables(windows) which was sent to HWC
 */
storage_of_drawables_rec hwc_storage_of_drawables;
#ifdef _F_PRESENT_SCANOUT_NOTIFY_
storage_of_drawables_rec hwc_previous_storage_of_drawables;
#endif //_F_PRESENT_SCANOUT_NOTIFY_

void
hwc_storage_clear( void )
{
    int i = 0;

    if( STORAGE_EMPTY == hwc_storage_of_drawables.count )
        return;

    /*
     * The storage does not do memory management so just initialize with NULL
     */

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
    for( i = 0; i < STORAGE_SIZE; i++)
        hwc_previous_storage_of_drawables.drawables[i] = NULL;

    hwc_previous_storage_of_drawables.count = hwc_storage_of_drawables.count;
#endif //_F_PRESENT_SCANOUT_NOTIFY_

    for( i = 0; i < STORAGE_SIZE; i++)
    {
#ifdef _F_PRESENT_SCANOUT_NOTIFY_
        hwc_previous_storage_of_drawables.drawables[i] = hwc_storage_of_drawables.drawables[i];
#endif //_F_PRESENT_SCANOUT_NOTIFY_

        hwc_storage_of_drawables.drawables[i] = NULL;
    }

    hwc_storage_of_drawables.count = STORAGE_EMPTY;
}

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
void
hwc_storage_remove_drawables (DrawablePtr drawable)
{
    int i = 0;

    if( STORAGE_EMPTY == hwc_storage_of_drawables.count)
        return;

    for( i = 0; i < hwc_storage_of_drawables.count; i++)
    {
        if (hwc_storage_of_drawables.drawables[i] == drawable)
            hwc_storage_of_drawables.drawables[i] = NULL;
    }
}
#endif //_F_PRESENT_SCANOUT_NOTIFY_

void
hwc_storage_add_drawables( DrawablePtr *drawables, int count )
{
    int i = 0;
    int j = 0;

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
    for ( i = 0; i < hwc_previous_storage_of_drawables.count; i++)
    {
        if (hwc_previous_storage_of_drawables.drawables[i])
        {
            for ( j = 0; j < count; j++)
            {
                if (hwc_previous_storage_of_drawables.drawables[i] == drawables[j])
                    break;
            }

            if (j == count)
            {
                if (hwc_previous_storage_of_drawables.drawables[i]->type == DRAWABLE_WINDOW)
                    present_send_scanout_notify ((WindowPtr)hwc_previous_storage_of_drawables.drawables[i], 0);
            }
        }
    }
#endif //_F_PRESENT_SCANOUT_NOTIFY_

    for( i = 0; i < count; i++)
    {
        hwc_storage_of_drawables.drawables[i] = drawables[i];

#ifdef _F_PRESENT_SCANOUT_NOTIFY_
        for ( j = 0; j < hwc_previous_storage_of_drawables.count; j++)
        {
            if (hwc_previous_storage_of_drawables.drawables[j] == drawables[i])
                break;
        }

        if (j == hwc_previous_storage_of_drawables.count)
        {
            if (hwc_storage_of_drawables.drawables[i]->type == DRAWABLE_WINDOW)
                present_send_scanout_notify ((WindowPtr)hwc_storage_of_drawables.drawables[i], 1);
        }
#endif //_F_PRESENT_SCANOUT_NOTIFY_
    }

    hwc_storage_of_drawables.count = count;
}

int
hwc_storage_is_empty( void )
{
    if( STORAGE_EMPTY == hwc_storage_of_drawables.count )
        return TRUE;
    return FALSE;
}


int hwc_validate_window(WindowPtr window)
{
    DrawablePtr drawable = NULL;
    int check = FALSE;
    int i = 0;

    if(hwc_storage_is_empty())
        return FALSE;

    for( i = 0; i < hwc_storage_of_drawables.count; i++ )
    {
        drawable = hwc_storage_of_drawables.drawables[i];
        if (drawable && DRAWABLE_WINDOW == drawable->type)
            if ( window == (WindowPtr) drawable)
            {
                check = TRUE;
                break;
            }
    }

    if(!check)
        return FALSE;

    return TRUE;
}

