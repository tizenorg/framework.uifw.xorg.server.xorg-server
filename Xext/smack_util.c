/*
 * Copyright (c) 2015 Samsung
 * Copyright (c) 2011 Intel Corporation
 * Copyright (c) 2011 Casey Schaufler
 *
 * Author: JengHyun Kang <jhyuni.kang@samsung.com>
 *             Casey Schaufler <casey@schaufler-ca.com>
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
#include "smack_util.h"

#include <fcntl.h>
#include <unistd.h>

char access_buff[ABSIZE];
int does_not_have_smack;

static inline char *
SmackUtilString(SmackUtilLabel *label)
{
    return (char *)&label->label;
}

static inline void
SmackUtilCopyString(SmackUtilLabel *to, const char *from)
{
    strncpy(SmackUtilString(to), from, SMACK_SIZE);
}

static int
SmackUtilHaveAccess (SmackUtilSubjectRec *subj, const char *object,
                 const char *access)
{
    int ret;
    int access_fd;

    if (does_not_have_smack)
        return 1;

    snprintf (access_buff, ABSIZE, "%s %s %s", (char *)&subj->smack, object, access);

    access_fd = open("/sys/fs/smackfs/access2", O_RDWR);
    if (access_fd < 0)
        access_fd = open("/smack/access2", O_RDWR);
    if (access_fd < 0)
    {
        ErrorF ("Smack access checking is unavailable.\n");
        does_not_have_smack = 1;
        return 1;
    }

    ret = write(access_fd, access_buff, strlen(access_buff) + 1);
    if (ret < 0)
    {
        ErrorF ("(\"%s\") fd=%d = %d \"%s\"\n",
                    access_buff, access_fd, ret, subj->command);
        close(access_fd);
        return -1;
    }

    ret = read(access_fd, access_buff, ABSIZE);
    if (ret < 0)
    {
        ErrorF ("(\"%s %s %s\") %d \"%s\"\n",
                    (char *)&subj->smack, object, access, ret, subj->command);
        close(access_fd);
        return -1;
    }
#ifdef SMACK_UTIL_DEBUG
    ErrorF ("(\"%s %s %s\") '%c' \"%s\"\n",
               (char *)&subj->smack, object, access, access_buff[0], subj->command);
#endif //SMACK_UTIL_DEBUG

    close(access_fd);

    return access_buff[0] == '1';
}

static int
SmackUtilDoCheck(SmackUtilSubjectRec *subj, const char *object, Mask mode)
{
    char *subject = SmackUtilString(&subj->smack);
    char access[6] = "-----";
    int rc;

    access[0] = (mode & DixReadAccess) ? 'r' : '-';
    access[1] = (mode & DixWriteAccess) ? 'w' : '-';

#ifdef SMACK_UTIL_DEBUG
    ErrorF("(\"%s %s %s\") \"%s\" privileged('%s')\n",
               (char *)&subj->smack, object, access, subj->command,
               (subj->privileged)?"yes":"no");
#endif //SMACK_UTIL_DEBUG

    /* Privileged subjects get access */
#ifndef _F_DO_NOT_PASS_PRIVILEGE_CHECK_FROM_ROOT_
    if (subj->privileged)
    {
        return Success;
    }
#endif //_F_PASS_PRIVILEGE_CHECK_FROM_ROOT_
    rc = SmackUtilHaveAccess(subj, object, access);
    if (rc <= 0)
        return BadValue;

    return Success;
}

static void
SmackUtilClientLabel(ClientPtr client, SmackUtilSubjectRec *subj)
{
    int fd = XaceGetConnectionNumber(client);
    const char *cmdname;
    Bool cached;
    pid_t pid;
    char path[SMACK_SIZE];
    struct ucred peercred;
    socklen_t len;
    int rc;

    /*
     * What to use where nothing can be discovered
     */
    SmackUtilCopyString(&subj->smack, "_");

    /*
     * First try is to get the Smack label from
     * the packet label.
     */
    len = SMACK_SIZE;
    rc = getsockopt(fd, SOL_SOCKET, SO_PEERSEC, path, &len);

    if (rc >= 0 && len > 0 && !(len == 1 && path[0] == '\0'))
    {
        path[len] = '\0';
        SmackUtilCopyString(&subj->smack, path);
    }

    len = sizeof(peercred);
    rc = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peercred, &len);
    if (rc >= 0)
    {
        if (peercred.uid == 0)
            subj->privileged = 1;
    }

    /* For local clients, try and determine the executable name */
    if (XaceIsLocal(client))
    {
        /* Get cached command name if CLIENTIDS is enabled. */
        cmdname = GetClientCmdName(client);
        cached = (cmdname != NULL);
        /* If CLIENTIDS is disabled, figure out the command name from
        * scratch. */
        if (!cmdname)
        {
            pid = DetermineClientPid(client);
            if (pid != -1)
                DetermineClientCmd(pid, &cmdname, NULL);
        }

        if (!cmdname)
            return;

        strncpy(subj->command, cmdname, COMMAND_LEN - 1);

        if (!cached)
            free((void *) cmdname); /* const char * */
    }
}

int
SmackUtilHaveAuthority(ClientPtr client, const char *object, Mask mode)
{
    int res = 0;
    SmackUtilSubjectRec subj;

    SmackUtilClientLabel(client, &subj);

    res = SmackUtilDoCheck(&subj, object, mode);

    return res;
}

