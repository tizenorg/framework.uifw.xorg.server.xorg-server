/*
 * Copyright 2011 Samsung Electronics co., Ltd. All Rights Reserved.
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


#ifndef _CONTAINER_H
#define _CONTAINER_H

//#define _GNU_SOURCE // for vasum.
#include "scrnintstr.h"

/* Extension info */
#define CONTAINER_EXTENSION_NAME       "Container"
#define CONTAINER_MAJOR_VERSION        1
#define CONTAINER_MINOR_VERSION        0
#define ContainerNumberEvents          0
#define ContainerNumberErrors          0

/*
 * Steal the SELinux private property.
 */
#define PRIVATE_XCONTAINER_CLIENT PRIVATE_XSELINUX
#define PRIVATE_XCONTAINER_RES PRIVATE_XSELINUX

typedef struct {
    int vsm_fd;
    void * vsm_ctx;
    int vsm_event_handle;
    int foreground_zone_id;

    CloseScreenProcPtr CloseScreen;
} ContainerScreenRec, *ContainerScreenPtr;

/* client info*/
typedef struct {
    unsigned int pid;
    unsigned int zone_id;
} ContainerClientRec, *ContainerClientPtr;

/* resource info */
typedef struct {
    unsigned int zone_id;
} ContainerResRec, *ContainerResPtr;

#endif /* _CONTAINER_H */
