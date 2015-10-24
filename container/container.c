/*
 * Copyright 2014 Samsung Electronics co., Ltd. All Rights Reserved.
 *
 * Contact:
 *     SooChan Lim <sc1.lim@samsung.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * this permission notice appear in supporting documentation.  This permission
 * notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <stdio.h>
#include <stdarg.h>

#include <X11/Xdefs.h>
#include <X11/Xatom.h>
#include "selection.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "propertyst.h"
#include "extnsionst.h"
#include "xacestr.h"
#include "client.h"
#include "../os/osdep.h"
#include "gcstruct.h"
#include "cursorstr.h"
#include "colormapst.h"
#include "container.h"
#include <vasum.h>

/* private state keys */
DevPrivateKeyRec containerClientKeyRec;
DevPrivateKeyRec containerScreenKeyRec;
DevPrivateKeyRec containerResKeyRec;

#define containerClientKey (&containerClientKeyRec)
#define containerScreenKey (&containerScreenKeyRec)
#define containerResKey (&containerResKeyRec)

#define GetContainerScreen(s) ((ContainerScreenPtr) \
    dixLookupPrivate(&(s)->devPrivates, containerScreenKey))

//#define CONTAINER_DEBUG 1

static char *str_res[11] = {
    "null",
    "RT_WINDOW",
    "RT_PIXMAP",
    "RT_GC",
    "RT_FONT",
    "RT_CURSOR",
    "RT_COLORMAP",
    "RT_CMAPENTRY",
    "RT_OTHERCLIENT",
    "RT_PASSIVEGRAB",
    "RT_LASTPREDEF",
};

static char *
_get_str_res(RESTYPE rtype)
{
    char *str = NULL;

    switch (rtype) {
        case RT_WINDOW:
            str = str_res[1];
            break;
        case RT_PIXMAP:
            str = str_res[2];
            break;
        case RT_GC:
            str = str_res[3];
            break;
        case RT_FONT:
            str = str_res[4];
            break;
        case RT_CURSOR:
            str = str_res[5];
            break;
        case RT_COLORMAP:
            str = str_res[6];
            break;
        case RT_CMAPENTRY:
            str = str_res[7];
            break;
        case RT_OTHERCLIENT:
            str = str_res[8];
            break;
        case RT_PASSIVEGRAB:
            str = str_res[9];
            break;
        case RT_LASTPREDEF:
            str = str_res[10];
            break;
        default:
            str = str_res[0];
            break;
    }

    return str;
}

/*
 * DIX Callbacks
 */
static void
ContainerClientState(CallbackListPtr *pcbl, void * data, void * calldata)
{
    NewClientInfoRec *pci = calldata;
    ClientPtr pClient = pci->client;
    ScreenPtr pScreen = screenInfo.screens[0];
    ContainerClientPtr pClientInfo = NULL;
    ContainerScreenPtr pContainerScreen = NULL;
    vsm_zone_h zone = 0;

    pClientInfo = dixLookupPrivate(&pClient->devPrivates, containerClientKey);
    if (!pClientInfo) {
        ErrorF ("[Container] Error: no client info.\n");
        return;
    }

    pContainerScreen = GetContainerScreen(pScreen);
    if (!pContainerScreen) {
        ErrorF ("[Container] Error: no client info.\n");
        return;
    }

    switch (pClient->clientState) {
        case ClientStateInitial:       /* new client */
        case ClientStateRunning:
            pClientInfo->pid = DetermineClientPid(pClient);
            zone = vsm_lookup_zone_by_pid (pContainerScreen->vsm_ctx, pClientInfo->pid);
            pClientInfo->zone_id = vsm_get_zone_id(zone);
#if CONTAINER_DEBUG
            ErrorF ("[Container:%s,%d] pid:%d, client_zone:%d\n", __func__, __LINE__, pClientInfo->pid, pClientInfo->zone_id);
#endif
            break;
        case ClientStateGone:       /* client disconnected */
        case ClientStateRetained:
            break;
        default:
            break;
    }
}

static void
ContainerClientAccess(CallbackListPtr *pcbl, void * data, void * calldata)
{
    XaceClientAccessRec *rec = calldata;
    ClientPtr pClient = rec->client;
    ClientPtr pTarget = rec->target;
    ContainerClientPtr pClientInfo = dixLookupPrivate(&pClient->devPrivates, containerClientKey);
    ContainerClientPtr pTargetInfo = dixLookupPrivate(&pTarget->devPrivates, containerClientKey);

    if (pClient == serverClient)
        return;

    /* zone_id=0 means that the client is host clients.
        The host client access all resources. */
    if (pClientInfo->zone_id == 0)
        return;

    if (pClientInfo->zone_id != pTargetInfo->zone_id) {
            rec->status = BadAccess;

            ErrorF ("[Container:%s,%d] Error: Wrong Access (client_zone:%d, target_zone_id:%d)\n",
                __func__, __LINE__, pClientInfo->zone_id, pTargetInfo->zone_id);
    }
}

static void
ContainerResourceAccess(CallbackListPtr *pcbl, void * data, void * calldata)
{
    XaceResourceAccessRec *rec = calldata;
    ClientPtr pClient = rec->client;
    ScreenPtr pScreen = screenInfo.screens[0];
    ContainerClientPtr pClientInfo = NULL;
    ContainerResPtr pResInfo = NULL;
    ContainerScreenPtr pContainerScreen = NULL;
    vsm_zone_h zone = 0;
    WindowPtr pWin = NULL;
    PixmapPtr pPix = NULL;
    GCPtr pGc = NULL;
    FontPtr pFont = NULL;
    CursorPtr pCursor = NULL;
    ColormapPtr pColormap = NULL;

    if (pClient == serverClient)
        return;

    /* compare the client zone_id with the resource zone_id */
    pClientInfo = dixLookupPrivate(&pClient->devPrivates, containerClientKey);

    /* zone_id=0 means that the client is host clients.
        The host client access all resources. */
    if (pClientInfo->zone_id == 0)
        return;

    pContainerScreen = GetContainerScreen(pScreen);
    if (!pContainerScreen) {
        ErrorF ("[Container] Error: no client info.\n");
        return;
    }

    switch (rec->rtype) {
        case RT_WINDOW:
            pWin = (WindowPtr)rec->res;
            pResInfo = dixLookupPrivate(&pWin->devPrivates, containerResKey);

            /* check the root window.
                 The root window access is dealt per request(api) level.
                 For example, resign the shmGetImage of root window.*/
            if (!pWin->parent) {
#if CONTAINER_DEBUG
                ErrorF("[Container] try to access the root window(id:%p).\n", rec->id);
                if (pClientInfo && pResInfo)
                    ErrorF ("[Container:%s,%d] pid:%d, client_zone:%d, res:%s, res_id:%p, res_zone:%d\n",
                        __func__, __LINE__, pClientInfo->pid, pClientInfo->zone_id, _get_str_res(rec->rtype), rec->id, pResInfo->zone_id);
#endif
                return;

            }
            break;
        case RT_PIXMAP:
            pPix = (PixmapPtr)rec->res;
            pResInfo = dixLookupPrivate(&pPix->devPrivates, containerResKey);
            break;
        case RT_GC:
            pGc = (GCPtr)rec->res;
            pResInfo = dixLookupPrivate(&pGc->devPrivates, containerResKey);
            break;
        case RT_FONT:
        case RT_CURSOR:
        case RT_COLORMAP:
        case RT_CMAPENTRY:
        case RT_OTHERCLIENT:
        case RT_PASSIVEGRAB:
        case RT_LASTPREDEF:
        default:
#if CONTAINER_DEBUG
            if (pClientInfo && pResInfo)
                ErrorF ("[Container:%s,%d] Not check resource : pid:%d, client_zone:%d, res:%s, res_id:%p, res_zone:%d\n",
                    __func__, __LINE__, pClientInfo->pid, pClientInfo->zone_id, _get_str_res(rec->rtype), rec->id, pResInfo->zone_id);
#endif
            /* do not check the null resources */
            return;
            break;
    }

    if (pClientInfo && pResInfo) {
        /* set zone_id to resource private at inital time to access resource(create reource..ResourceStateAdding) */
        if (!pResInfo->zone_id) {
            zone = vsm_lookup_zone_by_pid (pContainerScreen->vsm_ctx, pClientInfo->pid);
            pResInfo->zone_id = vsm_get_zone_id(zone);
        }

        /* check the wrong access with comparing zone_ids */
        if (pClientInfo->zone_id != pResInfo->zone_id) {
            rec->status = BadAccess;
            ErrorF ("[Container:%s,%d] Error: Wrong Access (pid:%d, client_zone:%d, res:%s, res_id:%p, res_zone:%d)\n",
                __func__, __LINE__, pClientInfo->pid, pClientInfo->zone_id, _get_str_res(rec->rtype), rec->id, pResInfo->zone_id);
        }
    }

#if CONTAINER_DEBUG
    if (pClientInfo && pResInfo)
        ErrorF ("[Container:%s,%d] pid:%d, client_zone:%d, res:%s, res_id:%p, res_zone:%d\n",
            __func__, __LINE__, pClientInfo->pid, pClientInfo->zone_id, _get_str_res(rec->rtype), rec->id, pResInfo->zone_id);
#endif
}

static void
ContainerSendAccess(CallbackListPtr *pcbl, void *unused, void *calldata)
{
    XaceSendAccessRec *rec = calldata;
    ClientPtr pClient = rec->client;
    ScreenPtr pScreen = screenInfo.screens[0];
    ContainerResPtr pResInfo = NULL;
    ContainerScreenPtr pContainerScreen = NULL;
    WindowPtr pWin = NULL;

    if (rec->client) {
        int i;

        for (i = 0; i < rec->count; i++)
            switch (rec->events[i].u.u.type)
            {
            case KeyPress:
            case KeyRelease:
            case ButtonPress:
            case ButtonRelease:
                pContainerScreen = GetContainerScreen(pScreen);
                pWin = (WindowPtr)rec->pWin;
                pResInfo = dixLookupPrivate(&pWin->devPrivates, containerResKey);
                if (pContainerScreen->foreground_zone_id != pResInfo->zone_id) {
                    rec->status = BadAccess;
                    ErrorF ("[Container:%s,%d] Error: Wrong Event Access (foreground_zone:%d, res_zone:%d)\n",
                        __func__, __LINE__, pContainerScreen->foreground_zone_id, pResInfo->zone_id);
                }

                break;
            case GenericEvent:
                // TODO: deal with generic events
                break;
            default:
                break;
            }
        }
}

static int check = 0;

static void
ConatinerWakeupHanlder(void * data, int err, void * p)
{
    fd_set *read_mask;
    ScreenPtr pScreen = NULL;
    ContainerScreenPtr pContainerScreen = NULL;
    int ret = VSM_ERROR_NONE;

    if (data == NULL || err < 0)
        return;

    pScreen = (ScreenPtr) data;
    pContainerScreen = GetContainerScreen(pScreen);
    if (!pContainerScreen)
        return;

    /* decalre files and link for x11 socket and files */
    if (!check) {
        const char *x11_unix = "/tmp/.X11-unix";
        const char *xim_unix = "/tmp/.XIM-unix";
        if (!access(x11_unix, R_OK) && !access(xim_unix, R_OK)) {
            ErrorF("#####[soolm] file exist.\n");
            const char *x0_lock = "/tmp/.X0-lock";
            const char *x11_unix_x0 = "/tmp/.X11-unix/X0";

            ret = vsm_declare_link(pContainerScreen->vsm_ctx, x0_lock, x0_lock);
            if (ret != VSM_ERROR_NONE) {
                ErrorF("[Container]: Error. vsm_declare_link.(%s)\n", x0_lock);
            }

            //files
            ret = vsm_declare_file(pContainerScreen->vsm_ctx, VSM_FSO_TYPE_DIR, x11_unix, O_CREAT|O_RDWR, 0777);
            if (ret != VSM_ERROR_NONE) {
                ErrorF("[Container]: Error. vsm_declare_file.(%s)\n", x11_unix);
            }
            // links
            ret = vsm_declare_link(pContainerScreen->vsm_ctx, x11_unix_x0, x11_unix_x0);
            if (ret != VSM_ERROR_NONE) {
                ErrorF("[Container]: Error. vsm_declare_link.(%s)\n", x11_unix_x0);
            }

            //files
            ret = vsm_declare_file(pContainerScreen->vsm_ctx, VSM_FSO_TYPE_DIR, xim_unix, O_CREAT|O_RDWR, 0777);
            if (ret != VSM_ERROR_NONE) {
                ErrorF("[Container]: Error. vsm_declare_file.(%s)\n", xim_unix);
            }

            check = 1;
        }
    }

    read_mask = p;
    if (FD_ISSET (pContainerScreen->vsm_fd, read_mask)) {
        vsm_enter_eventloop(pContainerScreen->vsm_ctx, 0, 0);
    }
}

static int
ProcContainerDispatch(ClientPtr client)
{
#ifdef CONTAINER_DEBUG
    REQUEST(xReq);

    ErrorF("%s(%d)\n", __func__, stuff->data);
#endif /* CONTAINER_DEBUG */
    return 0;
}

static int
SProcContainerDispatch(ClientPtr client)
{
#ifdef CONTAINER_DEBUG
    REQUEST(xReq);

    ErrorF("%s(%d)\n", __func__, stuff->data);
#endif /* CONTAINER_DEBUG */
    return 0;
}

static void
ContainerResetProc(ExtensionEntry *extEntry)
{
#ifdef CONTAINER_DEBUG
    ErrorF("%s(...)\n", __func__);
#endif /* CONTAINER_DEBUG */
}

static Bool
containerCloseScreen(ScreenPtr pScreen)
{
    ContainerScreenPtr cs = GetContainerScreen(pScreen);
    Bool ret;

    vsm_del_event_callback(cs->vsm_ctx, cs->vsm_event_handle);
    vsm_cleanup_context(cs->vsm_ctx);

    pScreen->CloseScreen = cs->CloseScreen;

    free(cs);
    dixSetPrivate(&pScreen->devPrivates, containerScreenKey, NULL);
    ret = (*pScreen->CloseScreen) (pScreen);

    return ret;
}

static int
ContainerVsmZoneEvent (vsm_zone_h zone, vsm_zone_event_t event, void *user_data)
{
    ScreenPtr pScreen = screenInfo.screens[0];
    ContainerScreenPtr pContainerScreen = NULL;
    int foreground_zone_id = -1;
    vsm_zone_h vsm_zone;

    /* we care about only foreground switch */
    if (event != VSM_ZONE_EVENT_SWITCHED)
        return TRUE;

    if (GetContainerScreen(pScreen))
        return FALSE;

    vsm_zone = vsm_get_foreground (pContainerScreen->vsm_ctx);
    foreground_zone_id = vsm_get_zone_id (vsm_zone);
    if (foreground_zone_id != pContainerScreen->foreground_zone_id) {
        ErrorF ("[Container:%s,%d] Change the foreground zone id (before : %d ==> after : %d)\n",
            __func__, __LINE__, pContainerScreen->foreground_zone_id, foreground_zone_id);

        pContainerScreen->foreground_zone_id = foreground_zone_id;
    }

    return TRUE;
}


static Bool
containerScreenInit(ScreenPtr pScreen)
{
    ContainerScreenPtr cs;
    int vsm_fd;
    vsm_context_h vsm_ctx;
    vsm_zone_h vsm_zone;
    int vsm_event_handle;

    vsm_ctx = vsm_create_context();
    if (vsm_ctx == NULL) {
        ErrorF ("[Container] Error : fail to create vsm_ctx.\n");
        return FALSE;
    }

    vsm_fd = vsm_get_poll_fd (vsm_ctx);
    if (vsm_fd < 0) {
        ErrorF ("[Container] Error : fail to get vsm_ctx fd.\n");
        return FALSE;
    }

    vsm_event_handle = vsm_add_event_callback(vsm_ctx, ContainerVsmZoneEvent, NULL);

    if (!dixRegisterPrivateKey(containerScreenKey, PRIVATE_SCREEN, 0))
        return FALSE;

    if (GetContainerScreen(pScreen))
        return TRUE;

    cs = (ContainerScreenPtr) calloc(1, sizeof(ContainerScreenRec));
    if (!cs)
        return FALSE;

    cs->vsm_fd = vsm_fd;
    cs->vsm_ctx = vsm_ctx;
    cs->vsm_event_handle = vsm_event_handle;
    vsm_zone = vsm_get_foreground (vsm_ctx);
    cs->foreground_zone_id = vsm_get_zone_id (vsm_zone);

    cs->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = containerCloseScreen;

    dixSetPrivate(&pScreen->devPrivates, containerScreenKey, cs);

    AddGeneralSocket(vsm_fd);

    RegisterBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
                                   ConatinerWakeupHanlder, pScreen);

    return TRUE;
}


void
ContainerExtensionInit(void)
{
    ExtensionEntry *extEntry;
    int ret = TRUE;
    ScreenPtr pScreen = NULL;

    /* Allocate private storage */
    if (!dixRegisterPrivateKey(containerClientKey, PRIVATE_XCONTAINER_CLIENT, sizeof(ContainerClientRec)))
        FatalError("Container: Failed to allocate private container client storage.\n");
    if (!dixRegisterPrivateKey(containerResKey, PRIVATE_XCONTAINER_RES, sizeof(ContainerResRec)))
        FatalError("Container: Failed to allocate private container client storage.\n");

    /* vsm context is only one in xserver. Therefore, store the vsm context information in the screen 0. */
    pScreen = screenInfo.screens[0];
    if (!containerScreenInit(pScreen))
        FatalError("[Container] Error : Failed to initialize screen private.\n");

    /* Register callbacks */
    ret &= AddCallback(&ClientStateCallback, ContainerClientState, NULL);
    ret &= XaceRegisterCallback(XACE_CLIENT_ACCESS, ContainerClientAccess, NULL);
    ret &= XaceRegisterCallback(XACE_RESOURCE_ACCESS, ContainerResourceAccess, NULL);
    ret &= XaceRegisterCallback(XACE_SEND_ACCESS, ContainerSendAccess, NULL);

#if 0
    ret &= XaceDeleteCallback (XACE_CORE_DISPATCH, ContainerCoreEvents, NULL);
    ret &= XaceDeleteCallback (XACE_EXT_DISPATCH, ContainerExtEvents, NULL);
    ret &= XaceDeleteCallback (XACE_AUDIT_END, ContainerAuditEndEvents, NULL);

//    ret &= XaceRegisterCallback(XACE_DEVICE_ACCESS, ContainerDevice, NULL);
//    ret &= XaceRegisterCallback(XACE_PROPERTY_ACCESS, ContainerProperty, NULL);
//    ret &= XaceRegisterCallback(XACE_RECEIVE_ACCESS, ContainerReceive, NULL);
//    ret &= XaceRegisterCallback(XACE_EXT_ACCESS, ContainerExtension, NULL);
//    ret &= XaceRegisterCallback(XACE_SERVER_ACCESS, ContainerServer, NULL);
//    ret &= XaceRegisterCallback(XACE_SELECTION_ACCESS, ContainerSelection, NULL);
//    ret &= XaceRegisterCallback(XACE_SCREEN_ACCESS, ContainerScreen, NULL);
//    ret &= XaceRegisterCallback(XACE_SCREENSAVER_ACCESS, ContainerScreen, NULL);
#endif

    if (!ret)
        FatalError("[Container] Error : Failed to register one or more callbacks\n");

    extEntry = AddExtension(CONTAINER_EXTENSION_NAME,
            ContainerNumberEvents, ContainerNumberErrors,
            ProcContainerDispatch, SProcContainerDispatch,
            ContainerResetProc, StandardMinorOpcode);

    if (!extEntry)
        FatalError("[Container] Error : Failed to add extension.\n");

}

