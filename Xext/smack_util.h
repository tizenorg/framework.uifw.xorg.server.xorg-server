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


#ifndef _SMACK_UTIL_H
#define _SMACK_UTIL_H

#define COMMAND_LEN 64
#define SMACK_SIZE 256
#define ABSIZE SMACK_SIZE + SMACK_SIZE + 10

typedef struct {
    CARD8   label[SMACK_SIZE];
} SmackUtilLabel;

/* subject state (clients and devices only) */
typedef struct {
    SmackUtilLabel smack;
    char command[COMMAND_LEN];
    int privileged;
} SmackUtilSubjectRec;

int SmackUtilHaveAuthority(ClientPtr client, const char *object, Mask mode);

#endif /* _SMACK_UTIL_H */
