
#ifdef HAVE_XORG_CONFIG_H
    #include <xorg-config.h>
#endif

#include "hwa_priv.h"

#include <syncsrv.h>
#include <unistd.h>
#include <xace.h>

#define VERIFY_RR_CRTC(id, ptr, a)\
    {\
	int rc = dixLookupResourceByType((void **)&(ptr), id,\
	                                 RRCrtcType, client, a);\
	if (rc != Success) {\
	    client->errorValue = id;\
	    return rc;\
	}\
    }

#define VERIFY_CRTC_OR_NONE(crtc_ptr, crtc_id, client, access) do {     \
        if ((crtc_id) == None)                                          \
            (crtc_ptr) = NULL;                                          \
        else {                                                          \
            VERIFY_RR_CRTC(crtc_id, crtc_ptr, access);                  \
        }                                                               \
    } while (0)


#define ENABLE 	1
#define DISABLE 0
#define IS_ON  1
#define IS_OFF 0

/* -------------------------------- Proc --------------------------------------- */

/*
 *  Main query version request
 *
 * This request returns the extension Major and Minor versions to a application
 */
static int
proc_hwa_query_version( ClientPtr client )
{
    REQUEST(xHWAQueryVersionReq);
    xHWAQueryVersionReply rep = {
        .type = X_Reply,
        .sequenceNumber = client->sequence,
        .length = 0,
        .majorVersion = HWA_MAJOR,
        .minorVersion = HWA_MINOR
    };

    REQUEST_SIZE_MATCH(xHWAQueryVersionReq);
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
proc_hwa_cursor_enable(ClientPtr client)
{
    REQUEST(xHWACursorEnableReq);
    xHWACursorEnableReply rep = {
            .type = X_Reply,
            .sequenceNumber = client->sequence,
            .length = 0,
            .status = 0
        };

    int status,i;
    ScreenPtr pScreen = NULL;
    CARD16 enable = 0;
    enable = (CARD16) stuff->enable;

    REQUEST_SIZE_MATCH(xHWACursorEnableReq);

    for (i = 0; i < screenInfo.numScreens; i++) {
        pScreen = screenInfo.screens[i];
        }

    if(!pScreen)
        return FALSE;
    status = hwa_CursorEnable( pScreen, &enable);
    if (status != Success)
        return status;

    rep.status = enable;
    (void) stuff;
    if (client->swapped) {
        swaps(&rep.status);
        swaps(&rep.sequenceNumber);
        swapl(&rep.length);
    }

    WriteToClient(client, sizeof (rep), &rep);

    return Success;
}

static int
proc_hwa_cursor_rotate(ClientPtr client)
{
    REQUEST(xHWACursorRotateReq);
    xHWACursorRotateReply rep = {
            .type = X_Reply,
            .sequenceNumber = client->sequence,
            .length = 0,
            .status = 0,
            .degree = 0
        };

    int status,i;
    ScreenPtr pScreen = NULL;
    CARD16 degree = stuff->degree;

    RRCrtc crtc_id = stuff->crtc_id;

    REQUEST_SIZE_MATCH(xHWACursorRotateReq);

    for (i = 0; i < screenInfo.numScreens; i++) {
        pScreen = screenInfo.screens[i];
    }


    if(!pScreen)
        return FALSE;
    status = hwa_CursorRotate( pScreen, crtc_id, &degree);
    if (status != Success)
        return status;


    rep.status = (CARD16)status;
    rep.degree = (CARD16)degree;

    if (client->swapped) {
        swaps(&rep.status);
        swaps(&rep.degree);
        swaps(&rep.sequenceNumber);
        swapl(&rep.length);
    }

    WriteToClient(client, sizeof (rep), &rep);

    return Success;
}

static int
proc_hwa_screen_invert_negative( ClientPtr client )
{
    RRCrtcPtr target_crtc = NULL;

    int accessibility_status = 0;
    int status = 0;

    REQUEST(xHWAScreenInvertNegativeReq);
    xHWAScreenInvertNegativeReply rep = {
        .type = X_Reply,
        .sequenceNumber = client->sequence,
        .length = 0,
        .status = 0,
    };

    REQUEST_SIZE_MATCH(xHWAScreenInvertNegativeReq);

    if( stuff->enable )
        accessibility_status = ENABLE;
    else
        accessibility_status = DISABLE;


    VERIFY_CRTC_OR_NONE(target_crtc, stuff->crtc_id, client, DixReadAccess);

    status = hwa_screen_invert_negative( target_crtc, accessibility_status );
    if( status != Success )
    	return BadRequest;

    if (client->swapped) {
        swaps(&rep.sequenceNumber);
        swapl(&rep.length);
    }

    if( accessibility_status )
        rep.status = IS_ON;
    else
        rep.status = IS_OFF;


    WriteToClient(client, sizeof(rep), &rep);
    return Success;
}

static int
proc_hwa_screen_scale( ClientPtr client )
{
    RRCrtcPtr target_crtc = NULL;

    int scale_status = 0;
    int status = 0;

    REQUEST(xHWAScreenScaleReq);
    xHWAScreenScaleReply rep = {
        .type = X_Reply,
        .sequenceNumber = client->sequence,
        .length = 0,
        .status = 0,
    };

    REQUEST_SIZE_MATCH(xHWAScreenScaleReq);

    if( stuff->enable )
    	scale_status = ENABLE;
    else
    	scale_status = DISABLE;

    VERIFY_CRTC_OR_NONE(target_crtc, stuff->crtc_id, client, DixReadAccess);

    status = hwa_screen_scale( target_crtc,
                               scale_status,
                               stuff->x,
                               stuff->y,
                               stuff->w,
                               stuff->h);
    if( status != Success )
    	return BadRequest;

    if (client->swapped) {
        swaps(&rep.sequenceNumber);
        swapl(&rep.length);
    }

    if( scale_status )
        rep.status = IS_ON;
    else
        rep.status = IS_OFF;

    WriteToClient(client, sizeof(rep), &rep);
    return Success;
}

static int
proc_hwa_show_layer(ClientPtr client)
{
    REQUEST(xHWAShowLayerReq);

    int status,i;
    ScreenPtr pScreen = NULL;
    CARD16 ovrlayer = stuff->ovrlayer;

    RRCrtc crtc_id = stuff->crtc_id;

    REQUEST_SIZE_MATCH(xHWAShowLayerReq);

    for (i = 0; i < screenInfo.numScreens; i++) {
        pScreen = screenInfo.screens[i];
    }

    if(!pScreen)
        return FALSE;

    status = hwa_ShowLayer( pScreen, crtc_id, ovrlayer );
    if (status != Success)
        return status;

    return Success;
}

static int
proc_hwa_hide_layer(ClientPtr client)
{
    REQUEST(xHWAHideLayerReq);

    int status,i;
    ScreenPtr pScreen = NULL;
    CARD16 ovrlayer = stuff->ovrlayer;

    RRCrtc crtc_id = stuff->crtc_id;

    REQUEST_SIZE_MATCH(xHWAHideLayerReq);

    for (i = 0; i < screenInfo.numScreens; i++) {
        pScreen = screenInfo.screens[i];
    }


    if(!pScreen)
        return FALSE;

    status = hwa_HideLayer( pScreen, crtc_id, ovrlayer );
    if (status != Success)
        return status;

    return Success;
}

/*
 *  proc_hwa_vector stores Main request handlers
 */
int
(*proc_hwa_vector[HWANumberRequests]) (ClientPtr) = {
    proc_hwa_query_version,             /* 0 */
    proc_hwa_cursor_enable,             /* 1 */
    proc_hwa_cursor_rotate,             /* 2 */
    proc_hwa_screen_invert_negative,    /* 3 */
    proc_hwa_screen_scale,              /* 4 */
    proc_hwa_show_layer,                /* 5 */
    proc_hwa_hide_layer,                /* 6 */
};

/*
 * MainProc function ( See AddExtension functions )
 *
 * It is used to initialize the hwa extension's main request handlers
 */
int
proc_hwa_dispatch(ClientPtr client)
{
    REQUEST(xReq);
    if (stuff->data >= HWANumberRequests || !proc_hwa_vector[stuff->data])
        return BadRequest;
    return (*proc_hwa_vector[stuff->data]) (client);
}

/* ------------------------------- End Proc ------------------------------------ */



/* -------------------------------- SProc -------------------------------------- */

/*
 *  Auxiliary query version request:
 *
 *    This function prepares some request data and then call
 *    the Main query version request
 */
static int
sproc_hwa_query_version(ClientPtr client)
{
    REQUEST( xHWAQueryVersionReq );

    swaps(&stuff->length);
    swapl(&stuff->majorVersion);
    swapl(&stuff->minorVersion);
    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

static int sproc_hwa_cursor_enable(ClientPtr client)
{
    REQUEST( xHWACursorEnableReq );

    swaps(&stuff->length);
    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

static int sproc_hwa_cursor_rotate(ClientPtr client)
{
    REQUEST( xHWACursorRotateReq );

    swaps(&stuff->length);
    swapl(&stuff->crtc_id);
    swaps(&stuff->degree);
    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

static int
sproc_hwa_screen_invert_negative(ClientPtr client)
{
    REQUEST( xHWAScreenInvertNegativeReq );

    swaps(&stuff->length);
    swapl(&stuff->crtc_id);

    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

static int
sproc_hwa_screen_scale(ClientPtr client)
{
    REQUEST( xHWAScreenScaleReq );

    swaps(&stuff->length);
    swapl(&stuff->crtc_id);
    swapl(&stuff->x);
    swapl(&stuff->y);
    swapl(&stuff->w);
    swapl(&stuff->h);

    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

static int sproc_hwa_show_layer(ClientPtr client)
{
    REQUEST( xHWAShowLayerReq );

    swaps(&stuff->length);
    swapl(&stuff->crtc_id);
    swaps(&stuff->ovrlayer);
    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

static int sproc_hwa_hide_layer(ClientPtr client)
{
    REQUEST( xHWAHideLayerReq );

    swaps(&stuff->length);
    swapl(&stuff->crtc_id);
    swaps(&stuff->ovrlayer);
    return (*proc_hwa_vector[stuff->hwaReqType]) (client);
}

/*
 *  sproc_hwa_vector stores Auxiliary request handlers
 */
int (*sproc_hwa_vector[HWANumberRequests]) (ClientPtr) = {
    sproc_hwa_query_version,           /* 0 */
    sproc_hwa_cursor_enable,           /* 1 */
    sproc_hwa_cursor_rotate,           /* 2 */
    sproc_hwa_screen_invert_negative,  /* 3 */
    sproc_hwa_screen_scale,            /* 4 */
    sproc_hwa_show_layer,              /* 5 */
    sproc_hwa_hide_layer,              /* 6 */
};


/*
 * SwappedMainProc function ( See AddExtension functions )
 *
 * It is used to initialize the hwa extension's auxiliary request handlers
 */
int
sproc_hwa_dispatch(ClientPtr client)
{
    REQUEST(xReq);
    if (stuff->data >= HWANumberRequests || !sproc_hwa_vector[stuff->data])
        return BadRequest;
    return (*sproc_hwa_vector[stuff->data]) (client);
}

/* ------------------------------- End Proc ------------------------------------ */

















