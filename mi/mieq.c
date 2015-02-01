/*
 *
Copyright 1990, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

/*
 * mieq.c
 *
 * Machine independent event queue
 *
 */

#if HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include   <X11/X.h>
#include   <X11/Xmd.h>
#include   <X11/Xproto.h>
#include   "misc.h"
#include   "windowstr.h"
#include   "pixmapstr.h"
#include   "inputstr.h"
#include   "inpututils.h"
#include   "mi.h"
#include   "mipointer.h"
#include   "scrnintstr.h"
#include   <X11/extensions/XI.h>
#include   <X11/extensions/XIproto.h>
#include   <X11/extensions/geproto.h>
#include   "extinit.h"
#include   "exglobals.h"
#include   "eventstr.h"

#ifdef DPMSExtension
#include "dpmsproc.h"
#include <X11/extensions/dpmsconst.h>
#endif

/* Maximum size should be initial size multiplied by a power of 2 */
#define QUEUE_INITIAL_SIZE                 256
#define QUEUE_RESERVED_SIZE                 64
#define QUEUE_MAXIMUM_SIZE                4096
#define QUEUE_DROP_BACKTRACE_FREQUENCY     100
#define QUEUE_DROP_BACKTRACE_MAX            10

#define EnqueueScreen(dev) dev->spriteInfo->sprite->pEnqueueScreen
#define DequeueScreen(dev) dev->spriteInfo->sprite->pDequeueScreen

static EventQueueRec miEventQueue;

#ifdef XQUARTZ
#include  <pthread.h>
static pthread_mutex_t miEventQueueMutex = PTHREAD_MUTEX_INITIALIZER;

extern BOOL serverRunning;
extern pthread_mutex_t serverRunningMutex;
extern pthread_cond_t serverRunningCond;

static inline void
wait_for_server_init(void)
{
    /* If the server hasn't finished initializing, wait for it... */
    if (!serverRunning) {
        pthread_mutex_lock(&serverRunningMutex);
        while (!serverRunning)
            pthread_cond_wait(&serverRunningCond, &serverRunningMutex);
        pthread_mutex_unlock(&serverRunningMutex);
    }
}
#endif

EventQueuePtr
mieqGetEventQueuePtr()
{
	return &miEventQueue;
}

size_t
mieqNumEnqueued(EventQueuePtr eventQueue)
{
    size_t n_enqueued = 0;

    if (eventQueue->nevents) {
        /* % is not well-defined with negative numbers... sigh */
        n_enqueued = eventQueue->tail - eventQueue->head + eventQueue->nevents;
        if (n_enqueued >= eventQueue->nevents)
            n_enqueued -= eventQueue->nevents;
    }
    return n_enqueued;
}

/* Pre-condition: Called with miEventQueueMutex held */
static Bool
mieqGrowQueue(EventQueuePtr eventQueue, size_t new_nevents)
{
    size_t i, n_enqueued, first_hunk;
    EventRec *new_events;

    if (!eventQueue) {
        ErrorF("[mi] mieqGrowQueue called with a NULL eventQueue\n");
        return FALSE;
    }

    if (new_nevents <= eventQueue->nevents)
        return FALSE;

    new_events = calloc(new_nevents, sizeof(EventRec));
    if (new_events == NULL) {
        ErrorF("[mi] mieqGrowQueue memory allocation error.\n");
        return FALSE;
    }

    n_enqueued = mieqNumEnqueued(eventQueue);

    /* We block signals, so an mieqEnqueue triggered by SIGIO does not
     * write to our queue as we are modifying it.
     */
    OsBlockSignals();

    /* First copy the existing events */
    first_hunk = eventQueue->nevents - eventQueue->head;
    memcpy(new_events,
           &eventQueue->events[eventQueue->head],
           first_hunk * sizeof(EventRec));
    memcpy(&new_events[first_hunk],
           eventQueue->events, eventQueue->head * sizeof(EventRec));

    /* Initialize the new portion */
    for (i = eventQueue->nevents; i < new_nevents; i++) {
        InternalEvent *evlist = InitEventList(1);

        if (!evlist) {
            size_t j;

            for (j = 0; j < i; j++)
                FreeEventList(new_events[j].events, 1);
            free(new_events);
            OsReleaseSignals();
            return FALSE;
        }
        new_events[i].events = evlist;
    }

    /* And update our record */
    eventQueue->tail = n_enqueued;
    eventQueue->head = 0;
    eventQueue->nevents = new_nevents;
    free(eventQueue->events);
    eventQueue->events = new_events;

    OsReleaseSignals();
    return TRUE;
}

Bool
mieqInit(void)
{
    memset(&miEventQueue, 0, sizeof(miEventQueue));
    miEventQueue.lastEventTime = GetTimeInMillis();

    if (!mieqGrowQueue(&miEventQueue, QUEUE_INITIAL_SIZE))
        FatalError("Could not allocate event queue.\n");

    SetInputCheck(&miEventQueue.head, &miEventQueue.tail);
    return TRUE;
}

void
mieqFini(void)
{
    int i;

    for (i = 0; i < miEventQueue.nevents; i++) {
        if (miEventQueue.events[i].events != NULL) {
            FreeEventList(miEventQueue.events[i].events, 1);
            miEventQueue.events[i].events = NULL;
        }
    }
    free(miEventQueue.events);
}

/* This function will determine if the given event is allowed to used the reserved
 * queue space.
 */
static Bool
mieqReservedCandidate(InternalEvent *e)
{
    switch (e->any.type) {
    case ET_KeyRelease:
    case ET_ButtonRelease:
#if XFreeXDGA
    case ET_DGAEvent:
#endif
    case ET_RawKeyRelease:
    case ET_RawButtonRelease:
    case ET_XQuartz:
        return TRUE;
    default:
        return FALSE;
    }
}

/*
 * Must be reentrant with ProcessInputEvents.  Assumption: mieqEnqueue
 * will never be interrupted.  If this is called from both signal
 * handlers and regular code, make sure the signal is suspended when
 * called from regular code.
 */

void
mieqEnqueue(DeviceIntPtr pDev, InternalEvent *e)
{
    unsigned int oldtail = miEventQueue.tail;
    InternalEvent *evt;
    int isMotion = 0;
    int evlen;
    Time time;
    size_t n_enqueued;

#ifdef XQUARTZ
    wait_for_server_init();
    pthread_mutex_lock(&miEventQueueMutex);
#endif

    verify_internal_event(e);

    n_enqueued = mieqNumEnqueued(&miEventQueue);

    /* avoid merging events from different devices */
    if (e->any.type == ET_Motion)
        isMotion = pDev->id;

    if (isMotion && isMotion == miEventQueue.lastMotion &&
        oldtail != miEventQueue.head) {
        oldtail = (oldtail - 1) % miEventQueue.nevents;
    }
    else if ((n_enqueued + 1 == miEventQueue.nevents) ||
             ((n_enqueued + 1 >= miEventQueue.nevents - QUEUE_RESERVED_SIZE) &&
              !mieqReservedCandidate(e))) {
        /* Toss events which come in late.  Usually this means your server's
         * stuck in an infinite loop somewhere, but SIGIO is still getting
         * handled.
         */
        miEventQueue.dropped++;
        if (miEventQueue.dropped == 1) {
            ErrorFSigSafe("[mi] EQ overflowing.  Additional events will be "
                         "discarded until existing events are processed.\n");
            xorg_backtrace();
            ErrorFSigSafe("[mi] These backtraces from mieqEnqueue may point to "
                         "a culprit higher up the stack.\n");
            ErrorFSigSafe("[mi] mieq is *NOT* the cause.  It is a victim.\n");
        }
        else if (miEventQueue.dropped % QUEUE_DROP_BACKTRACE_FREQUENCY == 0 &&
                 miEventQueue.dropped / QUEUE_DROP_BACKTRACE_FREQUENCY <=
                 QUEUE_DROP_BACKTRACE_MAX) {
            ErrorFSigSafe("[mi] EQ overflow continuing.  %u events have been "
                         "dropped.\n", miEventQueue.dropped);
            if (miEventQueue.dropped / QUEUE_DROP_BACKTRACE_FREQUENCY ==
                QUEUE_DROP_BACKTRACE_MAX) {
                ErrorFSigSafe("[mi] No further overflow reports will be "
                             "reported until the clog is cleared.\n");
            }
            xorg_backtrace();
        }

#ifdef XQUARTZ
        pthread_mutex_unlock(&miEventQueueMutex);
#endif
        return;
    }

    evlen = e->any.length;
    evt = miEventQueue.events[oldtail].events;
    memcpy(evt, e, evlen);

    time = e->any.time;
    /* Make sure that event times don't go backwards - this
     * is "unnecessary", but very useful. */
    if (time < miEventQueue.lastEventTime &&
        miEventQueue.lastEventTime - time < 10000)
        e->any.time = miEventQueue.lastEventTime;

    miEventQueue.lastEventTime = evt->any.time;
#ifdef _F_MIEQ_SPRITEINFO_NULL_CHECK_
    miEventQueue.events[oldtail].pScreen = ((pDev)&&(pDev->spriteInfo->sprite)) ? EnqueueScreen(pDev) : NULL;
#else
    miEventQueue.events[oldtail].pScreen = pDev ? EnqueueScreen(pDev) : NULL;
#endif//_F_MIEQ_SPRITEINFO_NULL_CHECK_

    miEventQueue.events[oldtail].pDev = pDev;
    miEventQueue.lastMotion = isMotion;
    miEventQueue.tail = (oldtail + 1) % miEventQueue.nevents;
#ifdef XQUARTZ
    pthread_mutex_unlock(&miEventQueueMutex);
#endif
}

/**
 * Changes the screen reference events are being enqueued from.
 * Input events are enqueued with a screen reference and dequeued and
 * processed with a (potentially different) screen reference.
 * This function is called whenever a new event has changed screen but is
 * still logically on the previous screen as seen by the client.
 * This usually happens whenever the visible cursor moves across screen
 * boundaries during event generation, before the same event is processed
 * and sent down the wire.
 *
 * @param pDev The device that triggered a screen change.
 * @param pScreen The new screen events are being enqueued for.
 * @param set_dequeue_screen If TRUE, pScreen is set as both enqueue screen
 * and dequeue screen.
 */
void
mieqSwitchScreen(DeviceIntPtr pDev, ScreenPtr pScreen, Bool set_dequeue_screen)
{
#ifdef XQUARTZ
    pthread_mutex_lock(&miEventQueueMutex);
#endif
    EnqueueScreen(pDev) = pScreen;
    if (set_dequeue_screen)
        DequeueScreen(pDev) = pScreen;
#ifdef XQUARTZ
    pthread_mutex_unlock(&miEventQueueMutex);
#endif
}

void
mieqSetHandler(int event, mieqHandler handler)
{
#ifdef XQUARTZ
    pthread_mutex_lock(&miEventQueueMutex);
#endif
    if (handler && miEventQueue.handlers[event])
        ErrorF("[mi] mieq: warning: overriding existing handler %p with %p for "
               "event %d\n", miEventQueue.handlers[event], handler, event);

    miEventQueue.handlers[event] = handler;
#ifdef XQUARTZ
    pthread_mutex_unlock(&miEventQueueMutex);
#endif
}

/**
 * Change the device id of the given event to the given device's id.
 */
static void
ChangeDeviceID(DeviceIntPtr dev, InternalEvent *event)
{
    switch (event->any.type) {
    case ET_Motion:
    case ET_KeyPress:
    case ET_KeyRelease:
    case ET_ButtonPress:
    case ET_ButtonRelease:
    case ET_ProximityIn:
    case ET_ProximityOut:
    case ET_Hierarchy:
    case ET_DeviceChanged:
    case ET_TouchBegin:
    case ET_TouchUpdate:
    case ET_TouchEnd:
        event->device_event.deviceid = dev->id;
        break;
    case ET_TouchOwnership:
        event->touch_ownership_event.deviceid = dev->id;
        break;
#if XFreeXDGA
    case ET_DGAEvent:
        break;
#endif
    case ET_RawKeyPress:
    case ET_RawKeyRelease:
    case ET_RawButtonPress:
    case ET_RawButtonRelease:
    case ET_RawMotion:
    case ET_RawTouchBegin:
    case ET_RawTouchEnd:
    case ET_RawTouchUpdate:
        event->raw_event.deviceid = dev->id;
        break;
    default:
        ErrorF("[mi] Unknown event type (%d), cannot change id.\n",
               event->any.type);
    }
}

static void
FixUpEventForMaster(DeviceIntPtr mdev, DeviceIntPtr sdev,
                    InternalEvent *original, InternalEvent *master)
{
    verify_internal_event(original);
    verify_internal_event(master);
    /* Ensure chained button mappings, i.e. that the detail field is the
     * value of the mapped button on the SD, not the physical button */
    if (original->any.type == ET_ButtonPress ||
        original->any.type == ET_ButtonRelease) {
        int btn = original->device_event.detail.button;

        if (!sdev->button)
            return;             /* Should never happen */

        master->device_event.detail.button = sdev->button->map[btn];
    }
}

/**
 * Copy the given event into master.
 * @param sdev The slave device the original event comes from
 * @param original The event as it came from the EQ
 * @param copy The event after being copied
 * @return The master device or NULL if the device is a floating slave.
 */
DeviceIntPtr
CopyGetMasterEvent(DeviceIntPtr sdev,
                   InternalEvent *original, InternalEvent *copy)
{
    DeviceIntPtr mdev;
    int len = original->any.length;
    int type = original->any.type;
    int mtype;                  /* which master type? */

    verify_internal_event(original);

    /* ET_XQuartz has sdev == NULL */
    if (!sdev || IsMaster(sdev) || IsFloating(sdev))
        return NULL;

#if XFreeXDGA
    if (type == ET_DGAEvent)
        type = original->dga_event.subtype;
#endif

    switch (type) {
    case ET_KeyPress:
    case ET_KeyRelease:
        mtype = MASTER_KEYBOARD;
        break;
    case ET_ButtonPress:
    case ET_ButtonRelease:
    case ET_Motion:
    case ET_ProximityIn:
    case ET_ProximityOut:
        mtype = MASTER_POINTER;
        break;
    default:
        mtype = MASTER_ATTACHED;
        break;
    }

    mdev = GetMaster(sdev, mtype);
    memcpy(copy, original, len);
    ChangeDeviceID(mdev, copy);
    FixUpEventForMaster(mdev, sdev, original, copy);

    return mdev;
}

static void
mieqMoveToNewScreen(DeviceIntPtr dev, ScreenPtr screen, DeviceEvent *event)
{
    if (dev && screen && screen != DequeueScreen(dev)) {
        int x = 0, y = 0;

        DequeueScreen(dev) = screen;
        x = event->root_x;
        y = event->root_y;
        NewCurrentScreen(dev, DequeueScreen(dev), x, y);
    }
}

/**
 * Post the given @event through the device hierarchy, as appropriate.
 * Use this function if an event must be posted for a given device during the
 * usual event processing cycle.
 */
void
mieqProcessDeviceEvent(DeviceIntPtr dev, InternalEvent *event, ScreenPtr screen)
{
    mieqHandler handler;
    DeviceIntPtr master;
    InternalEvent mevent;       /* master event */

    verify_internal_event(event);

    /* Custom event handler */
    handler = miEventQueue.handlers[event->any.type];

    switch (event->any.type) {
        /* Catch events that include valuator information and check if they
         * are changing the screen */
    case ET_Motion:
    case ET_KeyPress:
    case ET_KeyRelease:
    case ET_ButtonPress:
    case ET_ButtonRelease:
        if (!handler)
            mieqMoveToNewScreen(dev, screen, &event->device_event);
        break;
    case ET_TouchBegin:
    case ET_TouchUpdate:
    case ET_TouchEnd:
        if (!handler && (event->device_event.flags & TOUCH_POINTER_EMULATED))
            mieqMoveToNewScreen(dev, screen, &event->device_event);
        break;
    default:
        break;
    }
#ifdef _F_GESTURE_EXTENSION_
    switch (event->any.type)
    {
	case ET_MTSync:
		master = NULL;
		break;

	default:
	    master = CopyGetMasterEvent(dev, event, &mevent);

	    if (master)
        	master->lastSlave = dev;
    }
#else//_F_GESTURE_EXTENSION_
    master = CopyGetMasterEvent(dev, event, &mevent);

    if (master)
        master->lastSlave = dev;
#endif//_F_GESTURE_EXTENSION_

    /* If someone's registered a custom event handler, let them
     * steal it. */
    if (handler) {
        int screenNum = dev &&
            DequeueScreen(dev) ? DequeueScreen(dev)->myNum : (screen ? screen->
                                                              myNum : 0);
        handler(screenNum, event, dev);
        /* Check for the SD's master in case the device got detached
         * during event processing */
        if (master && !IsFloating(dev))
            handler(screenNum, &mevent, master);
    }
    else {
        /* process slave first, then master */
        dev->public.processInputProc(event, dev);

        /* Check for the SD's master in case the device got detached
         * during event processing */
        if (master && !IsFloating(dev))
            master->public.processInputProc(&mevent, master);
    }
}

/* Call this from ProcessInputEvents(). */
void
mieqProcessInputEvents(void)
{
    EventRec *e = NULL;
    ScreenPtr screen;
    static InternalEvent event;
    DeviceIntPtr dev = NULL, master = NULL;
    size_t n_enqueued;

#ifdef XQUARTZ
    pthread_mutex_lock(&miEventQueueMutex);
#endif

    /* Grow our queue if we are reaching capacity: < 2 * QUEUE_RESERVED_SIZE remaining */
    n_enqueued = mieqNumEnqueued(&miEventQueue);
    if (n_enqueued >= (miEventQueue.nevents - (2 * QUEUE_RESERVED_SIZE)) &&
        miEventQueue.nevents < QUEUE_MAXIMUM_SIZE) {
        ErrorF("[mi] Increasing EQ size to %lu to prevent dropped events.\n",
               miEventQueue.nevents << 1);
        if (!mieqGrowQueue(&miEventQueue, miEventQueue.nevents << 1)) {
            ErrorF("[mi] Increasing the size of EQ failed.\n");
        }
    }

    if (miEventQueue.dropped) {
        ErrorF("[mi] EQ processing has resumed after %lu dropped events.\n",
               miEventQueue.dropped);
        ErrorF
            ("[mi] This may be caused my a misbehaving driver monopolizing the server's resources.\n");
        miEventQueue.dropped = 0;
    }

    while (miEventQueue.head != miEventQueue.tail) {
        e = &miEventQueue.events[miEventQueue.head];

        event = *e->events;
        dev = e->pDev;
        screen = e->pScreen;

        miEventQueue.head = (miEventQueue.head + 1) % miEventQueue.nevents;

#ifdef XQUARTZ
        pthread_mutex_unlock(&miEventQueueMutex);
#endif

        master = (dev) ? GetMaster(dev, MASTER_ATTACHED) : NULL;

        if (screenIsSaved == SCREEN_SAVER_ON)
            dixSaveScreens(serverClient, SCREEN_SAVER_OFF, ScreenSaverReset);
#ifdef DPMSExtension
        else if (DPMSPowerLevel != DPMSModeOn)
            SetScreenSaverTimer();
#ifdef _F_DPMS_PHONE_CTRL_
        if (!DPMSPhoneCrtl)
            if (DPMSPowerLevel != DPMSModeOn)
                DPMSSet(serverClient, DPMSModeOn);
#else
        if (DPMSPowerLevel != DPMSModeOn)
            DPMSSet(serverClient, DPMSModeOn);
#endif
#endif

        mieqProcessDeviceEvent(dev, &event, screen);

        /* Update the sprite now. Next event may be from different device. */
        if (event.any.type == ET_Motion && master)
            miPointerUpdateSprite(dev);

#ifdef XQUARTZ
        pthread_mutex_lock(&miEventQueueMutex);
#endif
    }
#ifdef XQUARTZ
    pthread_mutex_unlock(&miEventQueueMutex);
#endif
}
